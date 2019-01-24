FROM alpine:3.8

EXPOSE 1812/udp 1813/udp 3000


RUN apk --no-cache add \
      wpa_supplicant freeradius freeradius-rest freeradius-eap openssl \
      ruby ruby-rdoc ruby-bundler ruby-ffi \
      ruby-dev make gcc libc-dev

WORKDIR /usr/src/healthcheck
COPY ./usr/src/healthcheck/Gemfile ./usr/src/healthcheck/Gemfile.lock ./usr/src/healthcheck/.ruby-version ./
RUN bundle check || bundle install
COPY ./usr/src/healthcheck/. .

# these were needed while bundling, but no longer
RUN apk del ruby-dev make gcc libc-dev

RUN mv /etc/raddb /etc/raddb.old

COPY etc /etc
COPY usr /usr

RUN mkdir /tmp/radiusd
RUN openssl dhparam -out /etc/raddb/certs/dh 1024


CMD [ "/bin/sh", "-c", \
  "wget $RADIUS_CONFIG_WHITELIST_URL -O /etc/raddb/clients.conf; \
   wget ${CERT_STORE_URL}/ca.pem -O /etc/raddb/certs/ca.pem; \
   wget ${CERT_STORE_URL}/comodoCA.pem -O /etc/raddb/certs/comodoCA.pem; \
   wget ${CERT_STORE_URL}/server.key -O /etc/raddb/certs/server.key; \
   wget ${CERT_STORE_URL}/server.pem -O /etc/raddb/certs/server.pem; \
   bundle exec rackup -o 0.0.0.0 -p 3000 & /usr/sbin/radiusd $RADIUSD_PARAMS | cat" \
]
