#!/bin/sh

if [ -n "${ECS_CONTAINER_METADATA_URI}" ]; then
  export TASK_ID=$(echo $(curl $ECS_CONTAINER_METADATA_URI | jq -r '.Labels ."com.amazonaws.ecs.task-arn"') | tr "/" "\n" | tail -n 1)
fi

c_rehash /etc/raddb/certs/trusted_certificates

# Start the healthcheck - with low priority
cd /healthcheck && nice -n 10 bundle exec puma -p 3000 &

# Start the freeradius prometheus stats exporter
nice -n 10 freeradius_exporter -web.listen-address 0.0.0.0:9812 &

# Run freeradius with any additonal arguments in foreground
# Note: the additional "exec" which replaces the shell with the command rather
# than leave a shell process running.
exec /usr/local/sbin/radiusd ${RADIUSD_PARAMS}
