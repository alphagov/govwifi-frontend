#!/usr/bin/env ash -x

set -o nounset -o errexit

ENDPOINT_ARG=${ENDPOINT_URL:+--endpoint-url=$ENDPOINT_URL}
CERTSDIR=${CERTSDIR:-/etc/raddb/certs}

aws ${ENDPOINT_ARG} s3 cp ${ALLOWLIST_BUCKET}/clients.conf ${CERTSDIR}/clients.conf
aws ${ENDPOINT_ARG} s3 cp ${CERT_STORE_BUCKET}/ca.pem ${CERTSDIR}/ca.pem
aws ${ENDPOINT_ARG} s3 cp ${CERT_STORE_BUCKET}/comodoCA.pem ${CERTSDIR}/comodoCA.pem
aws ${ENDPOINT_ARG} s3 cp ${CERT_STORE_BUCKET}/server.key ${CERTSDIR}/server.key
aws ${ENDPOINT_ARG} s3 cp ${CERT_STORE_BUCKET}/server.pem ${CERTSDIR}/server.pem

exec "$@"
