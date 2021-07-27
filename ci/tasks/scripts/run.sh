#!/bin/sh

bundle exec rackup -o 0.0.0.0 -p 3000 &
/usr/sbin/radiusd ${RADIUSD_PARAMS} &
freeradius_exporter -web.listen-address 0.0.0.0:9812
