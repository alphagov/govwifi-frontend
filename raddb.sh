#!/bin/ash -x

set -o nounset -o errexit

ENDPOINT_ARG=${ENDPOINT_URL:+--endpoint-url=$ENDPOINT_URL}
CERTSDIR=${CERTSDIR:-/etc/raddb/certs}

#Allowlist
aws ${ENDPOINT_ARG} s3 cp ${ALLOWLIST_BUCKET}/clients.conf ${CERTSDIR}/clients.conf

#Server-side key
aws ${ENDPOINT_ARG} s3 cp ${CERT_STORE_BUCKET}/server.key ${CERTSDIR}/server.key
aws ${ENDPOINT_ARG} s3 cp ${CERT_STORE_BUCKET}/server.pem ${CERTSDIR}/server.pem

#EAP-TLS certificates
aws ${ENDPOINT_ARG} s3 cp ${CERT_STORE_BUCKET}/${TRUSTED_CERTIFICATES_KEY} ${CERTSDIR}/certificates.zip

if ! (unzip -t ${CERTSDIR}/certificates.zip); then
  exit 1;
fi

unzip ${CERTSDIR}/certificates.zip -d ${CERTSDIR}/trusted_certificates
rm -f ${CERTSDIR}/certificates.zip

exec "$@"
