# WARNING: We must pin the Alpine version to 3.8 because freeradius-rest (r6) package in alpine-3.9 contains
# a regression breaking validation of rest authentication responses. Causes health checks to fail.
# This is a known issue with FreeRadius: https://github.com/FreeRADIUS/freeradius-server/issues/2821
FROM alpine:3.8

# Set up the radius configs. linux-headers and openssl-dev are needed for building Ruby.
RUN apk --no-cache add wpa_supplicant freeradius freeradius-rest freeradius-eap openssl make gcc libc-dev linux-headers openssl-dev \
 && mkdir -p /tmp/radiusd /etc/raddb \
 && openssl dhparam -out /etc/raddb/dh 1024
COPY radius /etc/raddb

# Install Ruby (for running the healthcheck service)
RUN wget https://cache.ruby-lang.org/pub/ruby/2.7/ruby-2.7.2.tar.gz \
  && tar xzvf ruby-2.7.2.tar.gz \
  && cd ruby-2.7.2 \
  && ./configure \
  && make \
  && make install

# Set up the healthcheck service
ARG BUNDLE_ARGS="--deployment --no-cache --no-prune --jobs=8 --without test"
WORKDIR /usr/src/healthcheck
COPY healthcheck/Gemfile* healthcheck/.ruby-version ./
# Bundler has to be installed now as we've built Ruby from source above.
RUN gem install bundler && bundle update --bundler
RUN bundle install $BUNDLE_ARGS \
 && apk del make gcc libc-dev
COPY healthcheck ./

VOLUME /etc/raddb/certs
EXPOSE 1812/udp 1813/udp 3000
CMD bundle exec rackup -o 0.0.0.0 -p 3000 & /usr/sbin/radiusd ${RADIUSD_PARAMS}
