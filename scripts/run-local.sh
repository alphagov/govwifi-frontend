#!/bin/sh

c_rehash /etc/raddb/certs/trusted_certificates

source /usr/bin/db_utils.sh
source /usr/bin/vars.sh

cd /api-stubs
export AUTH_DB="/tmp/auth.db"
export LOGGING_DB="/tmp/logging.db"
create_databases

cd /healthcheck && bundle exec rackup -o 0.0.0.0 -p 3000 &
cd /api-stubs && bundle exec rackup -o 0.0.0.0 -p 80 &
freeradius_exporter -web.listen-address 0.0.0.0:9812 &
/usr/local/sbin/radiusd -X &
/usr/bin/config_watch.py
