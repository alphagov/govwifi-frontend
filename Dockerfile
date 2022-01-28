# WARNING: We must pin the Alpine version to 3.8 because freeradius-rest (r6) package in alpine-3.9 contains
# a regression breaking validation of rest authentication responses. Causes health checks to fail.
# This is a known issue with FreeRadius: https://github.com/FreeRADIUS/freeradius-server/issues/2821
FROM ruby:2.6.3-alpine3.8

# Set up the radius configs
RUN apk --no-cache add wpa_supplicant freeradius freeradius-rest freeradius-eap openssl make gcc libc-dev curl jq \
 && mkdir -p /tmp/radiusd /etc/raddb \
 && openssl dhparam -out /etc/raddb/dh 1024
COPY radius /etc/raddb
COPY ci/tasks/scripts/run.sh /usr/bin

# Set up the healtcheck service
ARG BUNDLE_ARGS="--deployment --no-cache --no-prune --jobs=8 --without test"
WORKDIR /usr/src/healthcheck
COPY healthcheck/Gemfile* healthcheck/.ruby-version ./
RUN bundle install $BUNDLE_ARGS \
 && apk del make gcc libc-dev
COPY healthcheck ./

# Add freeradius exporter
RUN curl https://github.com/bvantagelimited/freeradius_exporter/releases/download/0.1.3/freeradius_exporter-0.1.3-amd64.tar.gz --location --output freeradius_exporter.tar.gz \
 && echo "151d5f8aa5e3084ebe315fd7ff5377d555ad12fa6a61180d9abd98d49e8cc342  freeradius_exporter.tar.gz" > checksums \
 && sha256sum -c checksums \
 && rm checksums \
 && tar -xzvf freeradius_exporter.tar.gz --strip-components=1 freeradius_exporter-0.1.3-amd64/freeradius_exporter \
 && mv freeradius_exporter /usr/sbin/freeradius_exporter \
 && chmod 755 /usr/sbin/freeradius_exporter

VOLUME /etc/raddb/certs
EXPOSE 1812/udp 1813/udp 3000 9812
CMD /usr/bin/run.sh
