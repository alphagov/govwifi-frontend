FROM ruby:2.7.2-alpine

# Set up the radius configs
RUN apk --no-cache add wpa_supplicant freeradius freeradius-rest freeradius-eap openssl make gcc libc-dev \
 && mkdir -p /tmp/radiusd /etc/raddb \
 && openssl dhparam -out /etc/raddb/dh 1024
COPY radius /etc/raddb

# Set up the healtcheck service
ARG BUNDLE_ARGS="--deployment --no-cache --no-prune --jobs=8 --without test"
WORKDIR /usr/src/healthcheck
COPY healthcheck/Gemfile* healthcheck/.ruby-version ./
RUN bundle install $BUNDLE_ARGS \
 && apk del make gcc libc-dev
COPY healthcheck ./

VOLUME /etc/raddb/certs
EXPOSE 1812/udp 1813/udp 3000
CMD bundle exec rackup -o 0.0.0.0 -p 3000 & /usr/sbin/radiusd ${RADIUSD_PARAMS}
