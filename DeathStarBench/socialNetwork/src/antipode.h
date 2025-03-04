#ifndef ANTIPODE_H
#define ANTIPODE_H

#include <string>
#include <fstream>
#include <iostream>

// mongolib
#include <bson/bson.h>
#include <mongoc/mongoc.h>
#include <nlohmann/json.hpp>

// to generate id
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// for change streams
#include <tbb/tbb.h>
#include <tbb/concurrent_hash_map.h>
#include <boost/thread.hpp>

// for serialization
#include <sstream>
#include <cereal/types/string.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/set.hpp>
#include <cereal/archives/json.hpp>

// DSB debug
#include "logger.h"


namespace antipode {

bool is_antipode_enabled() {
  return atoi(std::getenv("ANTIPODE")) != 0;
}

class Cscope {
  public:
    struct append_t {
      std::string wid;

      friend std::ostream& operator<<(std::ostream& os, append_t const& a) {
        os << "<#" << a.wid << ">";
      }

      std::string to_string(append_t const& a) {
        std::ostringstream ss;
        ss << a;
        return ss.str();
      }

      // This method lets cereal know which data members to serialize
      template<class Archive>
      void serialize(Archive& archive) {
        archive( CEREAL_NVP(wid) );
      }
    };

    std::string _id;
    std::list<append_t> _wid_list;

    Cscope();
    Cscope(std::string, std::list<append_t>); // copy of existing

    Cscope append(std::string);
    Cscope merge(Cscope cscope);

    friend std::ostream & operator<<(std::ostream &os, const Cscope& c) {
      os << " #" << c._id << " ; [";
      for (auto& a : c._wid_list) {
        os << a;
      }
      os << "]";
      return os;
    }

    friend class cereal::access;
    template <class Archive>
    void serialize(Archive& archive) {
      archive( CEREAL_NVP(_id), CEREAL_NVP(_wid_list) );
    }

    std::string to_json() {
      std::stringstream ss;
      {
        cereal::JSONOutputArchive out(ss);
        out(*this);
      } // archive goes out of scope, ensuring all contents are flushed
      return ss.str();
    }

    static Cscope from_json(std::string s) {
      // Dynamic exception type: nlohmann::detail::type_error
      Cscope c;
      std::stringstream ss;
      ss << s;
      {
        cereal::JSONInputArchive iarchive(ss); // Create an input archive
        iarchive(c); // Read the data from the archive
      } // archive goes out of scope, ensuring all contents are flushed
      return c;
    }
};

// constructors
Cscope::Cscope() {
  boost::uuids::uuid id = boost::uuids::random_generator()();
  _id = boost::uuids::to_string(id);
}
Cscope::Cscope(std::string id, std::list<append_t> wid_list) {
  _id = id;
  _wid_list = wid_list;
}

// API
Cscope Cscope::append(std::string wid) {
  append_t new_append;
  new_append.wid = wid;
  std::list<append_t> new_wid_list = _wid_list;
  new_wid_list.push_back(new_append);

  return Cscope(_id, new_wid_list);
}

//----------------------
// MongoDB
//----------------------

class AntipodeMongodb {
  static const std::string ANTIPODE_COLLECTION;
  static tbb::concurrent_hash_map<std::string, bool> cscope_change_stream_cache;

  mongoc_client_t* _client;
  std::string _dbname;
  mongoc_database_t* _db;
  mongoc_collection_t* _collection;

  public:
    AntipodeMongodb(mongoc_client_t*, std::string);
    ~AntipodeMongodb();
    void close();

    Cscope barrier(Cscope);

    //-----------
    // only needed for RP
    //-----------

    static void init_store(std::string, std::string);
    static void init_cscope_listener(std::string, std::string);

    bool close_scope(Cscope);
    Cscope retrieve_cscope(std::string);

  private:
    static void _init_cscope_listener(std::string, std::string);
    void _barrier_change_stream(std::string);
    void _barrier_change_stream_listener(std::string);
};
const std::string AntipodeMongodb::ANTIPODE_COLLECTION = "antipode";
tbb::concurrent_hash_map<std::string, bool> AntipodeMongodb::cscope_change_stream_cache;

AntipodeMongodb::AntipodeMongodb(mongoc_client_t* client, std::string dbname) {
  _client = client;
  _dbname = dbname;
  _db = mongoc_client_get_database(_client, _dbname.c_str());
  _collection = mongoc_database_get_collection (_db, AntipodeMongodb::ANTIPODE_COLLECTION.c_str());
}

AntipodeMongodb::~AntipodeMongodb() {
}

/* static */ void AntipodeMongodb::init_store (std::string uri, std::string dbname) {
  LOG(debug) << "[AntipodeMongodb] URI = " << uri;

  mongoc_init();
  mongoc_client_t* client = mongoc_client_new(uri.c_str());

  bson_error_t error;
  bson_t* b;

  // ensure client has connected
  b = BCON_NEW("ping", BCON_INT32(1));
  if(!mongoc_client_command_simple(client, dbname.c_str(), b, NULL, NULL, &error)){
    throw std::runtime_error("[AntipodeMongo] Failed to ping client" + std::string(error.message));
  }
  bson_free(b);

  // fetch server description
  mongoc_server_description_t* sd = mongoc_client_get_server_description(client, 1);
  if (sd == NULL) {
    throw std::runtime_error("[AntipodeMongo] Failed to fetch server description");
  }

  // look for is_master flag
  auto ismaster_description_json = bson_as_json(mongoc_server_description_ismaster(sd), nullptr);
  if (ismaster_description_json == NULL) {
    throw std::runtime_error("[AntipodeMongo] Unable to fetch cluster is_master flag");
  }
  mongoc_server_description_destroy(sd);

  // parse bson to json and get the ismaster flag
  bool is_master = nlohmann::json::parse(ismaster_description_json)["ismaster"];

  // only create collection on the master
  if (is_master) {
    LOG(debug) << "[AntipodeMongodb] Master found. Performing DB init calls";
    // ref: https://docs.mongodb.com/manual/core/transactions/#transactions-and-operations
    // ref: https://github.com/mongodb/mongo-c-driver/blob/master/src/libmongoc/examples/example-transaction.c
    mongoc_database_t* db = mongoc_client_get_database(client, dbname.c_str());

    // create collection for antipode `cscope_id`
    //    inserting into a nonexistent collection normally creates it, but a
    //    collection can't be created in a transaction - create it now
    mongoc_collection_t* collection = mongoc_database_create_collection(db, AntipodeMongodb::ANTIPODE_COLLECTION.c_str(), NULL, &error);
    if (!collection) {
      // code 48 is NamespaceExists, see error_codes.err in mongodb source
      if (error.code == 48) {
        collection = mongoc_database_get_collection (db, AntipodeMongodb::ANTIPODE_COLLECTION.c_str());
      } else {
        throw std::runtime_error("[AntipodeMongo] Failed to create collection: " + std::string(error.message));
      }
    }

    LOG(debug) << "[AntipodeMongodb] Created collection";

    // create unique index for CTX_ID
    bson_t keys;
    bson_init (&keys);

    BSON_APPEND_INT32(&keys, "cscope_id", 1);
    char* index_name = mongoc_collection_keys_to_index_string(&keys);
    bson_t* create_indexes = BCON_NEW (
        "createIndexes", BCON_UTF8(AntipodeMongodb::ANTIPODE_COLLECTION.c_str()),
        "indexes", "[", "{",
            "key", BCON_DOCUMENT (&keys),
            "name", BCON_UTF8 ("cscope_id"),
            "unique", BCON_BOOL(false),
        "}", "]");

    if (!mongoc_database_write_command_with_opts (db, create_indexes, NULL, NULL /* &reply */, &error)) {
      throw std::runtime_error("[AntipodeMongo] Error creating cscope_id Index: " + std::string(error.message));
    }

    LOG(debug) << "[AntipodeMongodb] Created Index";
    bson_free (index_name);
    bson_destroy (create_indexes);
    mongoc_database_destroy(db);
  }
  else {
    LOG(debug) << "[AntipodeMongodb] Skipped master actions";
  }

  mongoc_client_destroy (client);
  LOG(debug) << "[AntipodeMongodb] Finished Init Store";
}

bool AntipodeMongodb::close_scope(Cscope cscope) {
  bson_error_t error;
  bson_t *selector = BCON_NEW("cscope_id", BCON_UTF8(cscope._id.c_str()));

  bson_t *action = bson_new();
  bson_t action__set;
  BSON_APPEND_DOCUMENT_BEGIN(action, "$set", &action__set);
    BSON_APPEND_UTF8(&action__set, "object", cscope.to_json().c_str());
  bson_append_document_end(action, &action__set);

  bson_t *opts = BCON_NEW("upsert",  BCON_BOOL(true));

  // TODO: does this merge the document with different keys?
  bool r = mongoc_collection_update_one(_collection, selector, action, opts, NULL /* reply */, &error);
  if (!r) {
    MONGOC_ERROR ("[Antipode] Inject failed: %s", error.message);
  }
  return r;
}

Cscope AntipodeMongodb::retrieve_cscope(std::string cscope_id) {
  bson_t* filter = BCON_NEW ("cscope_id", BCON_UTF8(cscope_id.c_str()));
  bson_t* opts = BCON_NEW ("limit", BCON_INT64(1));
  mongoc_cursor_t* cursor;
  Cscope cscope;

  const bson_t* doc;
  cursor = mongoc_collection_find_with_opts(_collection, filter, opts, NULL);
  bool cscope_id_visible = mongoc_cursor_next(cursor, &doc);

  if (cscope_id_visible) {
    // -- debug
    // char* str = bson_as_canonical_extended_json (doc, NULL);
    // LOG(debug) << " IS_VISIBLE DONE: " << str;
    // bson_free (str);
    // --

    // -- Update with new cscope
    bson_iter_t cscope_iter;
    if (bson_iter_init_find (&cscope_iter, doc, "object") && BSON_ITER_HOLDS_UTF8 (&cscope_iter)) {
      std::string cscope_serialized(bson_iter_utf8(&cscope_iter, /* length */ NULL));
      cscope = Cscope::from_json(cscope_serialized);
    }
  }

  mongoc_cursor_destroy (cursor);
  bson_destroy (filter);
  bson_destroy (opts);

  return cscope;
}

Cscope AntipodeMongodb::barrier(Cscope cscope) {
  // now we check if all the objects inside at their respective datastores

  // add a read concern to opts
  // example: http://mongoc.org/libmongoc/current/mongoc_client_write_command_with_opts.html
  bson_t *read_opts = bson_new();
  mongoc_read_concern_t *read_concern = mongoc_read_concern_new();
  mongoc_read_concern_set_level (read_concern, MONGOC_READ_CONCERN_LEVEL_LOCAL);
  mongoc_read_concern_append (read_concern, read_opts);
  mongoc_read_prefs_t *read_prefs;
  read_prefs = mongoc_read_prefs_new(MONGOC_READ_SECONDARY);

  // We assume writes to post datastore only
  auto post_collection = mongoc_client_get_collection(_client, "post", "post");

  // We first query by context id
  bson_t *query_cid = bson_new();
  BSON_APPEND_UTF8(query_cid, "cid", cscope._id.c_str());
  bool read_cid = false;
  while(!read_cid) {
    mongoc_cursor_t *read_cid_cursor = mongoc_collection_find_with_opts(post_collection, query_cid, read_opts, read_prefs);
    const bson_t *read_cid_doc;
    read_cid = mongoc_cursor_next(read_cid_cursor, &read_cid_doc);

    bson_error_t cid_error;
    if (mongoc_cursor_error (read_cid_cursor, &cid_error)) {
      LOG(error) << "An error occurred: " << cid_error.message;
      continue;
    }
    LOG(error) << "[ANTIPODE] Was context #" << cscope._id << " found at " << std::getenv("ZONE") << " replica? " << read_cid;
    mongoc_cursor_destroy(read_cid_cursor);
  }
  bson_destroy(query_cid);

  //------
  // Now we search by object
  for (Cscope::append_t& a : cscope._wid_list) {
    // init query object
    bson_t *query = bson_new();
    bson_oid_t oid;
    bson_oid_init_from_string (&oid, a.wid.c_str());
    BSON_APPEND_OID(query, "_id", &oid);

    bool read_post = false;
    while(!read_post) {
      mongoc_cursor_t *read_cursor = mongoc_collection_find_with_opts(post_collection, query, read_opts, read_prefs);
      const bson_t *read_doc;
      read_post = mongoc_cursor_next(read_cursor, &read_doc);
      bson_error_t error;
      if (mongoc_cursor_error (read_cursor, &error)) {
        LOG(error) << "An error occurred: " << error.message;
        continue;
      }

      LOG(error) << "[ANTIPODE] Was post #" << a.wid << " found at " << std::getenv("ZONE") << " replica? " << read_post;
      mongoc_cursor_destroy(read_cursor);
    }
    bson_destroy(query);
  }

  mongoc_collection_destroy(post_collection);
  bson_destroy (read_opts);
  mongoc_read_prefs_destroy (read_prefs);
  mongoc_read_concern_destroy (read_concern);

  return cscope;
}

void AntipodeMongodb::close() {
  mongoc_collection_destroy(_collection);
  mongoc_database_destroy(_db);
}

//------------
// DEPRECATED
//------------

/* static */ void AntipodeMongodb::init_cscope_listener (std::string uri, std::string dbname) {
  boost::thread tserver(&AntipodeMongodb::_init_cscope_listener, uri, dbname);
  // tserver.join();
}
/* static */ void AntipodeMongodb::_init_cscope_listener (std::string uri, std::string dbname) {
  // init db and collection
  bson_error_t error;

  mongoc_init();
  mongoc_client_t* client = mongoc_client_new(uri.c_str());
  mongoc_database_t* db = mongoc_client_get_database(client, dbname.c_str());
  mongoc_collection_t* collection = mongoc_database_get_collection(db, AntipodeMongodb::ANTIPODE_COLLECTION.c_str());

  // refs:
  // http://mongoc.org/libmongoc/1.17.0/mongoc_change_stream_t.html
  // https://docs.mongodb.com/manual/changeStreams/
  bson_t *pipeline = BCON_NEW("pipeline",
    "[",
      "{",
        "$match", "{",
          "$and", "[",
            "{",
              "operationType", "insert",
            "}",
          "]",
        "}",
      "}",
    "]");

  // if the stream finds the same cscope
  mongoc_change_stream_t *stream;
  stream = mongoc_collection_watch (collection, pipeline, NULL /* opts */);
  while (true) {
    const bson_t *change;
    if (mongoc_change_stream_next (stream, &change)) {
      // for debug:
      // char *as_json = bson_as_relaxed_extended_json (change, NULL);
      // LOG(debug) << "CHANGE JSON: " << as_json;
      // bson_free (as_json);

      // parsing ref: http://mongoc.org/libbson/current/parsing.html
      bson_iter_t change_iter;
      bson_iter_t cscope_id_iter;

      if (bson_iter_init (&change_iter, change) && bson_iter_find_descendant (&change_iter, "fullDocument.cscope_id", &cscope_id_iter) && BSON_ITER_HOLDS_UTF8 (&cscope_id_iter)) {
        std::string cscope_id(bson_iter_utf8(&cscope_id_iter, /* length */ NULL));
        AntipodeMongodb::cscope_change_stream_cache.insert(std::make_pair(cscope_id, true));
      }
    }
  }

  // const bson_t *resume_token;
  // bson_error_t error;
  // if (mongoc_change_stream_error_document (stream, &error, NULL)) {
  //   MONGOC_ERROR ("%s\n", error.message);
  // }

  mongoc_change_stream_destroy (stream);
  bson_destroy(pipeline);
  mongoc_collection_destroy(collection);
  mongoc_database_destroy(db);
  mongoc_client_destroy (client);
}

void AntipodeMongodb::_barrier_change_stream(std::string cscope_id) {
  //------------
  // CHANGE STREAM
  //------------
  // refs:
  // http://mongoc.org/libmongoc/1.17.0/mongoc_change_stream_t.html
  // https://docs.mongodb.com/manual/changeStreams/
  mongoc_change_stream_t *stream;
  // bson_t pipeline = BSON_INITIALIZER;
  bson_t *pipeline = BCON_NEW("pipeline",
    "[",
      "{",
        "$match", "{",
          "$and", "[",
            "{",
              "fullDocument.cscope_id", BCON_UTF8(cscope_id.c_str()),
            "}",
            "{",
              "operationType", "insert",
            "}",
          "]",
        "}",
      "}",
    "]");
  const bson_t *change;

  // if the stream finds the same cscope
  stream = mongoc_collection_watch (_collection, pipeline, NULL /* opts */);
  while (mongoc_change_stream_next (stream, &change)) {
    break;
    // for debug:
    // char *as_json = bson_as_relaxed_extended_json (change, NULL);
    // fprintf (stderr, "CSCOPE %s IN DOCUMENT: %s\n", cscope_id.c_str(), as_json);
    // bson_free (as_json);

    // Got document:
    // {
    //   "_id" : { "_data" : "8260912B960000001E2B022C0100296E5A100497F661F4018447EEAE871045D76AB3E846645F6964006460912B96CE0EFD70017274DA0004" },
    //   "operationType" : "insert",
    //   "clusterTime" : { "$timestamp" : { "t" : 1620126614, "i" : 30 } },
    //   "fullDocument" : {
    //     "_id" : { "$oid" : "60912b96ce0efd70017274da" },
    //     "cscope_id" : "6548665420189724672"
    //   },
    //   "ns" : { "db" : "post", "coll" : "antipode" },
    //   "documentKey" : { "_id" : { "$oid" : "60912b96ce0efd70017274da" } }
    // }
  }
  LOG(debug) << " IS_VISIBLE " << cscope_id;

  // const bson_t *resume_token;
  // bson_error_t error;
  // if (mongoc_change_stream_error_document (stream, &error, NULL)) {
  //   MONGOC_ERROR ("%s\n", error.message);
  // }

  mongoc_change_stream_destroy (stream);
  bson_destroy(pipeline);
}

void AntipodeMongodb::_barrier_change_stream_listener(std::string cscope_id) {
  tbb::concurrent_hash_map<std::string, bool>::const_accessor read_lock;
  while(!AntipodeMongodb::cscope_change_stream_cache.find(read_lock, cscope_id));
  read_lock.release();


  // Display the occurrences
  // ref: https://www.inf.ed.ac.uk/teaching/courses/ppls/TBBtutorial.pdf
  // for(tbb::concurrent_hash_map<std::string, bool>::iterator i=cscope_change_stream_cache.begin(); i!=cscope_change_stream_cache.end(); ++i ) {
  //   printf("%s %d\n",i->first.c_str(),i->second);
  // }
}

} // namespace antipode

#endif //ANTIPODE_H
