FROM govwifi-frontend-base:latest

# Set up the healthcheck service
ARG BUNDLE_ARGS="--deployment --no-cache --no-prune --jobs=8 --without test"
WORKDIR /usr/src/healthcheck
COPY healthcheck/Gemfile* healthcheck/.ruby-version ./
# Bundler needs to be installed now as we've built Ruby from source (in base image).
RUN gem install bundler && bundle update --bundler
RUN bundle install $BUNDLE_ARGS \
 && apk del make gcc libc-dev
COPY healthcheck ./

VOLUME /etc/raddb/certs
EXPOSE 1812/udp 1813/udp 3000

CMD bundle exec rackup -o 0.0.0.0 -p 3000 & /usr/sbin/radiusd ${RADIUSD_PARAMS}
