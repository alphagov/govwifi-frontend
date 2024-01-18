#!/bin/sh

c_rehash /etc/raddb/certs/trusted_certificates

source /usr/bin/db_utils.sh
source /usr/bin/vars.sh

(
  delete_databases
  export AUTH_DB="/tmp/auth_test.db"
  export LOGGING_DB="/tmp/logging_test.db"
  create_databases
  cd /api-stubs
  bundle exec rspec
)

retVal=$?

if [ $retVal -ne 0 ]; then
  exit $retVal
fi

(
  export AUTH_DB="/tmp/auth.db"
  export LOGGING_DB="/tmp/logging.db"
  create_databases
  cd /api-stubs
  bundle exec rackup -o 0.0.0.0 -p 80 &
)

echo "start authentication and logging api stubs"
while [ $(curl --write-out '%{http_code}' --silent --output /dev/null 127.0.0.1:80/healthcheck) -ne 200 ]
do
  echo -n .
  sleep 2
done

echo "start radius server"
cd /healthcheck && bundle exec rackup -o 0.0.0.0 -p 3000 &
/usr/local/sbin/radiusd -X &
while [ $(curl --write-out '%{http_code}' --silent --output /dev/null 127.0.0.1:3000) -ne 200 ]
do
  echo -n .
  sleep 2
done

cd /test-app
export AUTH_DB="/tmp/auth.db"
export LOGGING_DB="/tmp/logging.db"
bundle exec rspec
