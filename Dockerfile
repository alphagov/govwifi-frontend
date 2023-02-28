FROM ruby:3.1.3-alpine3.15

# Set up the radius configs
RUN apk --no-cache add wpa_supplicant freeradius~=3.0.25 freeradius-rest freeradius-eap freeradius-radclient libxml2 \
    sqlite-libs sqlite sqlite-dev openssl make gcc libc-dev curl jq python3 py3-pip net-tools tmux \
    && mkdir -p /tmp/radiusd /etc/raddb \
    && openssl dhparam -out /etc/raddb/dh 1024
COPY radius /etc/raddb

# Add freeradius exporter
RUN curl https://github.com/bvantagelimited/freeradius_exporter/releases/download/0.1.3/freeradius_exporter-0.1.3-amd64.tar.gz --location --output freeradius_exporter.tar.gz \
 && echo "151d5f8aa5e3084ebe315fd7ff5377d555ad12fa6a61180d9abd98d49e8cc342  freeradius_exporter.tar.gz" > checksums \
 && sha256sum -c checksums \
 && rm checksums \
 && tar -xzvf freeradius_exporter.tar.gz --strip-components=1 freeradius_exporter-0.1.3-amd64/freeradius_exporter \
 && mv freeradius_exporter /usr/sbin/freeradius_exporter \
 && chmod 755 /usr/sbin/freeradius_exporter

RUN pip3 install watchdog==2.1.9
COPY config_watch.py /usr/bin
COPY ci/tasks/scripts/run*.sh ci/tasks/scripts/db_utils.sh ci/tasks/scripts/vars.sh /usr/bin/
RUN chmod 755 /usr/bin/*.sh

COPY api-stubs /api-stubs
WORKDIR /api-stubs
RUN bundle install $BUNDLE_ARGS

COPY test-app /test-app
WORKDIR /test-app
RUN bundle install $BUNDLE_ARGS

COPY healthcheck /healthcheck
WORKDIR /healthcheck
RUN bundle install $BUNDLE_ARGS

RUN apk del make gcc libc-dev

VOLUME /etc/raddb/certs
EXPOSE 1812/udp 1813/udp 3000 9812
CMD /usr/bin/run.sh
