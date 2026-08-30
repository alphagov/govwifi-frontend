#!/bin/sh

if [ -n "${ECS_CONTAINER_METADATA_URI}" ]; then
  export TASK_ID=$(echo $(curl $ECS_CONTAINER_METADATA_URI | jq -r '.Labels ."com.amazonaws.ecs.task-arn"') | tr "/" "\n" | tail -n 1)
fi

if [ "x${GOVLOGGER_FILE}" == "x" ]; then
    GOVLOGGER_FILE="/healthcheck/govlogs.log"
fi

if [ "x${ROTATED_GOVLOGGER_FILE}" == "x" ]; then
    ROTATED_GOVLOGGER_FILE="/healthcheck/govlogs.rotated"
fi

if [ "x${GOVLOGGER_LOG_PROG_COMMAND}" == "x" ]; then
    # Note this is without EAP-PEAR
    # GOVLOGGER_LOG_PROG_COMMAND="/usr/bin/process_gov_logs --delete --state /healthcheck/statefile --expires 5 --reduce_duplicates --reduce_freeradius_proxied --reduce_last_date_only --reduce_drop_eap_peap --logfile ${ROTATED_GOVLOGGER_FILE} >> /healthcheck/reduced_logs.out 2>>/healthcheck/process_gov_logs.err"
    # This is with
    GOVLOGGER_LOG_PROG_COMMAND="/usr/bin/process_gov_logs --delete --state /healthcheck/statefile --expires 5 --reduce_duplicates --reduce_freeradius_proxied --reduce_last_date_only --logfile ${ROTATED_GOVLOGGER_FILE} >> /healthcheck/reduced_logs.out 2>>/healthcheck/process_gov_logs.err"
fi

export GOVLOGGER_FILE
export ROTATED_GOVLOGGER_FILE
export GOVLOGGER_LOG_PROG_COMMAND

c_rehash /etc/raddb/certs/trusted_certificates

freeradius_exporter -web.listen-address 0.0.0.0:9812 &
cd /healthcheck && bundle exec puma -p 3000 &
exec /usr/local/sbin/radiusd ${RADIUSD_PARAMS}
