#!/bin/sh

source /usr/bin/db_utils.sh
source /usr/bin/vars.sh

(
  cd /api-stubs
  export AUTH_DB="/tmp/auth_test.db"
  export LOGGING_DB="/tmp/logging_test.db"
  create_databases
  bundle exec rspec
  delete_databases
)

(
  cd /api-stubs
  export AUTH_DB="/tmp/auth.db"
  export LOGGING_DB="/tmp/logging.db"
  create_databases
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
/usr/sbin/radiusd -X &
while [ $(curl --write-out '%{http_code}' --silent --output /dev/null 127.0.0.1:3000) -ne 200 ]
do
  echo -n .
  sleep 2
done

cd /test-app
export AUTH_DB="/tmp/auth.db"
export LOGGING_DB="/tmp/logging.db"
bundle exec rspec