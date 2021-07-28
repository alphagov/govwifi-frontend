#!/bin/sh

if [ -n "${ECS_CONTAINER_METADATA_URI}" ]; then
  export TASK_ID=$(curl $ECS_CONTAINER_METADATA_URI | jq -r '.Labels ."com.amazonaws.ecs.task-arn"')
fi

bundle exec rackup -o 0.0.0.0 -p 3000 &
/usr/sbin/radiusd ${RADIUSD_PARAMS} &
freeradius_exporter -web.listen-address 0.0.0.0:9812
