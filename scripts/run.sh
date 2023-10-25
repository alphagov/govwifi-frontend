#!/bin/sh

if [ -n "${ECS_CONTAINER_METADATA_URI}" ]; then
  export TASK_ID=$(echo $(curl $ECS_CONTAINER_METADATA_URI | jq -r '.Labels ."com.amazonaws.ecs.task-arn"') | tr "/" "\n" | tail -n 1)
fi

c_rehash /etc/raddb/certs/trusted_certificates

cd /healthcheck && bundle exec rackup -o 0.0.0.0 -p 3000 &
/usr/local/sbin/radiusd ${RADIUSD_PARAMS} &
freeradius_exporter -web.listen-address 0.0.0.0:9812
