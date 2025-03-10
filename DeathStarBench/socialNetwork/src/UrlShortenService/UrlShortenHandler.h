#ifndef SOCIAL_NETWORK_MICROSERVICES_SRC_URLSHORTENSERVICE_URLSHORTENHANDLER_H_
#define SOCIAL_NETWORK_MICROSERVICES_SRC_URLSHORTENSERVICE_URLSHORTENHANDLER_H_

#include <random>
#include <chrono>
#include <future>

#include <mongoc.h>
#include <libmemcached/memcached.h>
#include <libmemcached/util.h>
#include <bson/bson.h>

#include "../../gen-cpp/UrlShortenService.h"
#include "../../gen-cpp/ComposePostService.h"
#include "../ClientPool.h"
#include "../ThriftClient.h"
#include "../logger.h"
#include "../tracing.h"
#include <xtrace/xtrace.h>
#include <xtrace/baggage.h>

#define HOSTNAME "http://short-url/"

namespace social_network {

class UrlShortenHandler : public UrlShortenServiceIf {
 public:
  UrlShortenHandler(memcached_pool_st *, mongoc_client_pool_t *,
      ClientPool<ThriftClient<ComposePostServiceClient>> *);
  ~UrlShortenHandler() override = default;

  void UploadUrls(UrlListRpcResponse &, int64_t,
      const std::vector<std::string> &,
      const std::map<std::string, std::string> &) override;

  void GetExtendedUrls(UrlListRpcResponse &, int64_t,
                       const std::vector<std::string> &,
                       const std::map<std::string, std::string> &) override ;

 private:
  memcached_pool_st *_memcached_client_pool;
  mongoc_client_pool_t *_mongodb_client_pool;
  ClientPool<ThriftClient<ComposePostServiceClient>> *_compose_client_pool;
  static std::mt19937 _generator;
  static std::uniform_int_distribution<int> _distribution;
  static std::string _GenRandomStr(int length);
};

std::mt19937 UrlShortenHandler::_generator = std::mt19937(
    std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_int_distribution<int> UrlShortenHandler::_distribution =
    std::uniform_int_distribution<int>(0, 61);

UrlShortenHandler::UrlShortenHandler(
    memcached_pool_st *memcached_client_pool,
    mongoc_client_pool_t *mongodb_client_pool,
    ClientPool<ThriftClient<ComposePostServiceClient>> *compose_client_pool) {
  _compose_client_pool = compose_client_pool;
  _memcached_client_pool = memcached_client_pool;
  _mongodb_client_pool = mongodb_client_pool;
}

std::string UrlShortenHandler::_GenRandomStr(int length) {
  const char char_map[] = "abcdefghijklmnopqrstuvwxyzABCDEF"
                    "GHIJKLMNOPQRSTUVWXYZ0123456789";
  std::string return_str;
  for (int i = 0; i < length; ++i) {
    return_str.append(1, char_map[_distribution(_generator)]);
  }
  return return_str;
}
void UrlShortenHandler::UploadUrls(
    UrlListRpcResponse& response,
    int64_t req_id,
    const std::vector<std::string> &urls,
    const std::map<std::string, std::string> &carrier) {

  std::vector<std::string> _return;
  std::map<std::string, std::string>::const_iterator baggage_it = carrier.find("baggage");
  if (baggage_it != carrier.end()) {
    SET_CURRENT_BAGGAGE(Baggage::deserialize(baggage_it->second));
  }

  // XTRACE("UrlShortenHandler::UploadUrls", {{"RequestID", std::to_string(req_id)}});

  // Initialize a span
  TextMapReader reader(carrier);
  std::map<std::string, std::string> writer_text_map;
  TextMapWriter writer(writer_text_map);
  auto parent_span = opentracing::Tracer::Global()->Extract(reader);
  auto span = opentracing::Tracer::Global()->StartSpan(
      "UploadUrls",
      { opentracing::ChildOf(parent_span->get()) });
  opentracing::Tracer::Global()->Inject(span->context(), writer);

  std::vector<Url> target_urls;
  std::future<void> mongo_future;

  Baggage mongo_baggage;
  if (!urls.empty()) {
    for (auto &url : urls) {
      Url new_target_url;
      new_target_url.expanded_url = url;
      new_target_url.shortened_url = HOSTNAME +
          UrlShortenHandler::_GenRandomStr(10);
      target_urls.emplace_back(new_target_url);
      _return.emplace_back(new_target_url.shortened_url);
    }

    mongo_baggage = BRANCH_CURRENT_BAGGAGE();
    mongo_future = std::async(
        std::launch::async, [&](){
          BAGGAGE(mongo_baggage);  // automatically set / reinstate baggage on destructor

          mongoc_client_t *mongodb_client = mongoc_client_pool_pop(
              _mongodb_client_pool);
          if (!mongodb_client) {
            ServiceException se;
            se.errorCode = ErrorCode::SE_MONGODB_ERROR;
            se.message = "Failed to pop a client from MongoDB pool";
            throw se;
          }
          auto collection = mongoc_client_get_collection(
              mongodb_client, "url-shorten", "url-shorten");
          if (!collection) {
            ServiceException se;
            se.errorCode = ErrorCode::SE_MONGODB_ERROR;
            se.message = "Failed to create collection user from DB user";
            mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
            throw se;
          }

          mongoc_bulk_operation_t *bulk;
          bson_t *doc;
          bson_error_t error;
          bson_t reply;
          bool ret;
          bulk = mongoc_collection_create_bulk_operation_with_opts(
              collection, nullptr);
          for (auto &url : target_urls) {
            doc = bson_new();
            BSON_APPEND_UTF8(doc, "shortened_url", url.shortened_url.c_str());
            BSON_APPEND_UTF8(doc, "expanded_url", url.expanded_url.c_str());
            mongoc_bulk_operation_insert (bulk, doc);
            bson_destroy(doc);
          }
          ret = mongoc_bulk_operation_execute (bulk, &reply, &error);
          if (!ret) {
            LOG(error) << "MongoDB error: "<< error.message;
            ServiceException se;
            se.errorCode = ErrorCode::SE_MONGODB_ERROR;
            se.message = "Failed to insert urls to MongoDB";
            bson_destroy (&reply);
            mongoc_bulk_operation_destroy(bulk);
            mongoc_collection_destroy(collection);
            mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
            // XTRACE("Failed to insert urls to MongoDB");
            throw se;
          }
          bson_destroy (&reply);
          mongoc_bulk_operation_destroy(bulk);
          mongoc_collection_destroy(collection);
          mongoc_client_pool_push(_mongodb_client_pool, mongodb_client);
        });
  }

  Baggage compose_baggage = BRANCH_CURRENT_BAGGAGE();
  std::future<void> compose_future = std::async(
      std::launch::async, [&]() {
        BAGGAGE(compose_baggage);  // automatically set / reinstate baggage on destructor

        // Upload to compose post service
        auto compose_post_client_wrapper = _compose_client_pool->Pop();
        if (!compose_post_client_wrapper) {
          ServiceException se;
          se.errorCode = ErrorCode::SE_THRIFT_CONN_ERROR;
          se.message = "Failed to connected to compose-post-service";
          // XTRACE("Failed to connect to compose-post-service");
          throw se;
        }
        auto compose_post_client = compose_post_client_wrapper->GetClient();
        try {
          writer_text_map["baggage"] = BRANCH_CURRENT_BAGGAGE().str();
          BaseRpcResponse cp_response;
          compose_post_client->UploadUrls(cp_response, req_id, target_urls, writer_text_map);
          Baggage b = Baggage::deserialize(response.baggage);
          JOIN_CURRENT_BAGGAGE(b);
        } catch (...) {
          _compose_client_pool->Push(compose_post_client_wrapper);
          LOG(error) << "Failed to upload urls to compose-post-service";
          // XTRACE("Failed to upload urls to compose-post-service");
          throw;
        }
        _compose_client_pool->Push(compose_post_client_wrapper);
      });

  if (!urls.empty()) {
    try {
      mongo_future.get();
      JOIN_CURRENT_BAGGAGE(mongo_baggage);
    } catch (...) {
      LOG(error) << "Failed to upload shortened urls from MongoDB";
      // XTRACE("Failed to upload shortened urls from MongoDB");
      throw;
    }
  }


  try {
    compose_future.get();
    JOIN_CURRENT_BAGGAGE(compose_baggage);
  } catch (...) {
    LOG(error) << "Failed to upload shortened urls from compose-post-service";
    // XTRACE("Failed to upload shortened urls from compose-post-service");
    throw;
  }

  span->Finish();

  // XTRACE("TextHandler::UploadText complete");

  response.baggage = GET_CURRENT_BAGGAGE().str();
  response.result = _return;
  DELETE_CURRENT_BAGGAGE();

}
void UrlShortenHandler::GetExtendedUrls(
    UrlListRpcResponse& response,
    int64_t req_id,
    const std::vector<std::string> &shortened_id,
    const std::map<std::string, std::string> &carrier) {

  // TODO: Implement GetExtendedUrls
}

}



#endif //SOCIAL_NETWORK_MICROSERVICES_SRC_URLSHORTENSERVICE_URLSHORTENHANDLER_H_
