#!/bin/sh

function create_databases () {
    touch $AUTH_DB $LOGGING_DB
    sqlite3 $AUTH_DB 'create table lines (line TEXT);'
    sqlite3 $LOGGING_DB 'create table lines (username TEXT, mac TEXT, called_station_id TEXT, site_ip_address TEXT,
                                             cert_name TEXT, authentication_result TEXT, "Access-Accept" TEXT,
                                             authentication_reply TEXT, task_id TEXT, cert_serial TEXT, cert_subject TEXT, cert_issuer TEXT);'
}

function delete_databases () {
  rm -rf $AUTH_DB $LOGGING_DB
}


