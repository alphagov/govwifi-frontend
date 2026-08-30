
# This builds the freeradius binary package and creates a tar archive. The files for the package
# where discovered by running the build and looking at differences before and after the make install steps

FROM ruby:3.4.7-alpine3.22 AS freeradius_binary

RUN apk --update --no-cache add wpa_supplicant openssl make gcc libc-dev curl talloc-dev jq g++ zlib-dev \
    openssl-dev ca-certificates linux-headers python3 py3-pip py3-wheel net-tools tmux sqlite-libs \
    sqlite sqlite-dev libxml2 curl-dev json-c-dev libmemcached-dev \
    mariadb-connector-c-dev py-watchdog git

RUN wget https://github.com/FreeRADIUS/freeradius-server/archive/refs/tags/release_3_2_10.tar.gz && \
    tar xzf release_3_2_10.tar.gz && \
    rm release_3_2_10.tar.gz && \
    mv freeradius-server-release_3_2_10 freeradius-server

COPY rlm_govlogger_module freeradius-server
WORKDIR /freeradius-server

# We add in the govlogger module into the stable modules
RUN echo rlm_govlogger >> src/modules/stable

# Then add the tests to the all.mk file at top level
RUN sed -E -i 's/^(SUBMAKEFILES .*)/\1 govlogger\/all.mk/' src/tests/all.mk

RUN ./configure CPPFLAGS=-DX509_V_FLAG_PARTIAL_CHAIN=1 --sysconfdir=/etc --without-ruby
RUN make
RUN make tests.govlogger
RUN make install
RUN cd / && tar czf /root/freeradius.tgz \
    etc/raddb \
    usr/local/bin/dhcpclient \
    usr/local/bin/map_unit \
    usr/local/bin/radattr \
    usr/local/bin/radclient \
    usr/local/bin/radcrypt \
    usr/local/bin/radeapclient \
    usr/local/bin/radlast \
    usr/local/bin/radsecret \
    usr/local/bin/radsqlrelay \
    usr/local/bin/radtest \
    usr/local/bin/radwho \
    usr/local/bin/radzap \
    usr/local/bin/rlm_sqlippool_tool \
    usr/local/bin/smbencrypt \
    usr/local/lib/libfreeradius* \
    usr/local/lib/proto_dhcp.a \
    usr/local/lib/proto_dhcp.la \
    usr/local/lib/proto_dhcp.so \
    usr/local/lib/proto_vmps.a \
    usr/local/lib/proto_vmps.la \
    usr/local/lib/proto_vmps.so \
    usr/local/lib/rlm_* \
    usr/local/sbin/checkrad \
    usr/local/sbin/raddebug \
    usr/local/sbin/radiusd \
    usr/local/sbin/radmin \
    usr/local/sbin/rc.radiusd \
    usr/local/share/freeradius \
    usr/local/var/log/radius/radacct
CMD /bin/sh

#
# This creates the freeradius_exporter image
#

FROM ruby:3.4.7-alpine3.22 AS freeradius_exporter

RUN apk --update --no-cache add curl

RUN curl https://github.com/bvantagelimited/freeradius_exporter/releases/download/0.1.6/freeradius_exporter-0.1.6-amd64.tar.gz --location --output freeradius_exporter.tar.gz
RUN echo '38bf31e6a35e2afe0959e9f6eb90b1beb6b84e32d02448ee1005093ee322f173 freeradius_exporter.tar.gz' > checksums
RUN sha256sum -c checksums
RUN rm checksums
RUN tar -xzvf freeradius_exporter.tar.gz --strip-components=1 freeradius_exporter-0.1.6-amd64/freeradius_exporter
RUN mv freeradius_exporter /usr/sbin/freeradius_exporter
RUN chmod 755 /usr/sbin/freeradius_exporter

#
# This is the actual container build, which copies artifacts from the freeradius_binary and freeradius_exporter images
#

FROM ruby:3.4.7-alpine3.22

RUN apk --update --no-cache add wpa_supplicant openssl make gcc libc-dev curl talloc-dev jq g++ zlib-dev \
    openssl-dev ca-certificates linux-headers python3 py3-pip py3-wheel net-tools tmux sqlite-libs \
    sqlite sqlite-dev libxml2 curl-dev json-c-dev libmemcached-dev \
    mariadb-connector-c-dev py-watchdog perl perl-json perl-json-xs

COPY --from=freeradius_binary /root/freeradius.tgz /root/freeradius.tgz
RUN cd / && tar xvzf /root/freeradius.tgz

RUN rm -rf /etc/raddb/mods-enabled/* /etc/raddb/sites-enabled/* /etc/raddb/dh && \
    openssl dhparam -out /etc/raddb/dh 1024 && \
    mkdir -p /tmp/radiusd
COPY radius /etc/raddb

COPY --from=freeradius_exporter /usr/sbin/freeradius_exporter /usr/sbin/freeradius_exporter

COPY config_watch.py /usr/bin
COPY scripts/run*.sh scripts/db_utils.sh scripts/vars.sh scripts/process_gov_logs /usr/bin/

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
