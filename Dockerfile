FROM ruby:3.2.2-alpine

RUN apk --update --no-cache add wpa_supplicant openssl make gcc libc-dev curl talloc-dev jq g++ zlib-dev \
                                openssl-dev linux-headers python3 py3-pip py3-wheel net-tools tmux sqlite-libs \
                                sqlite sqlite-dev libxml2 curl-dev json-c-dev libmemcached-dev \
                                mariadb-connector-c-dev py-watchdog

RUN wget https://github.com/FreeRADIUS/freeradius-server/releases/download/release_3_2_2/freeradius-server-3.2.2.tar.gz \
    && tar xzvf freeradius-server-3.2.2.tar.gz \
    && cd freeradius-server-3.2.2 \
    && ./configure CPPFLAGS=-DX509_V_FLAG_PARTIAL_CHAIN=1 --sysconfdir=/etc \
    && make \
    && make install
RUN rm -rf ./freeradius-server-3.2.2

RUN rm -rf /etc/raddb/mods-enabled/* /etc/raddb/sites-enabled/* /etc/raddb/dh && \
    openssl dhparam -out /etc/raddb/dh 1024 && \
    mkdir -p /tmp/radiusd
COPY radius /etc/raddb

RUN curl https://github.com/bvantagelimited/freeradius_exporter/releases/download/0.1.6/freeradius_exporter-0.1.6-amd64.tar.gz --location --output freeradius_exporter.tar.gz \
 && echo "38bf31e6a35e2afe0959e9f6eb90b1beb6b84e32d02448ee1005093ee322f173  freeradius_exporter.tar.gz" > checksums \
 && sha256sum -c checksums \
 && rm checksums \
 && tar -xzvf freeradius_exporter.tar.gz --strip-components=1 freeradius_exporter-0.1.6-amd64/freeradius_exporter \
 && mv freeradius_exporter /usr/sbin/freeradius_exporter \
 && chmod 755 /usr/sbin/freeradius_exporter

COPY config_watch.py /usr/bin
COPY scripts/run*.sh scripts/db_utils.sh scripts/vars.sh /usr/bin/
RUN chmod 755 /usr/bin/*.sh

COPY api-stubs /api-stubs
WORKDIR /api-stubs
RUN bundle config force_ruby_platform true
RUN bundle install

COPY test-app /test-app
WORKDIR /test-app
RUN bundle config force_ruby_platform true
RUN bundle install

COPY healthcheck /healthcheck
WORKDIR /healthcheck
RUN bundle install

RUN rm -rf /etc/raddb/certs
VOLUME /etc/raddb/certs
EXPOSE 1812/udp 1813/udp 3000 9812
CMD /usr/bin/run.sh
