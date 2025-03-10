#!/bin/bash
echo "*********************************"
echo "Starting the replica set"
echo "*********************************"

dockerize -wait tcp://post-storage-mongodb-eu:27017 -wait tcp://post-storage-mongodb-us:27017 -wait-retry-interval 30s -timeout 300s echo "[INFO] post-storage-mongodb cluster ready!"

mongo mongodb://post-storage-mongodb-eu:27017 replicaSet.js
done=$?
if [ "$done" -ne 0 ]; then
  echo "*********************************"
  echo "FAILED to load replica set configuration!"
  echo "*********************************"
  exit $done
fi

mongo mongodb://post-storage-mongodb-us:27017 --eval "rs.secondaryOk()"
done=$?
if [ "$done" -ne 0 ]; then
  echo "*********************************"
  echo "FAILED to set secondaryOK!"
  echo "*********************************"
  exit $done
fi

echo "*********************************"
echo "Replica set DONE!"
echo "*********************************"

echo "*********************************"
echo "More MongoDB settings"
echo "*********************************"
mongo mongodb://post-storage-mongodb-eu:27017 --eval "db.adminCommand( { setParameter: 1, enableFlowControl: false  } )"
mongo mongodb://post-storage-mongodb-us:27017 --eval "db.adminCommand( { setParameter: 1, enableFlowControl: false  } )"

echo "*********************************"
echo "Opening HTTP:8000 server for dockerize coordination"
echo "*********************************"

ran -p 8000 -l -r /tmp/

echo "*********************************"
echo "HTTP:8000 server for dockerize coordination DONE!"
echo "*********************************"
