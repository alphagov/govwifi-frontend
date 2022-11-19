ifeq ($(shell uname -m) ,arm64)
	DOCKER_FLAGS += --platform linux/amd64
endif

test: build FORCE
	docker run $(DOCKER_FLAGS) --rm -v=$(CURDIR)/radius:/etc/raddb -v=$(CURDIR)/radius/certs:/etc/raddb/certs -p 1812-1813:1812-1813/udp -p 3000:3000 -p 9812:9812 --name govwifi-frontend-c govwifi-frontend /usr/bin/run-tests.sh

lint: build
	docker run $(DOCKER_FLAGS) --rm -v=$(CURDIR)/radius:/etc/raddb -v=$(CURDIR)/radius/certs:/etc/raddb/certs -p 1812-1813:1812-1813/udp -p 3000:3000 -p 9812:9812 govwifi-frontend /bin/sh -c "cd /healthcheck && bundle exec rubocop -d"

build: FORCE
	docker build $(DOCKER_FLAGS) -t govwifi-frontend .

serve: build FORCE
	# Declare the /etc/radius/certs mount explicity, despite being a subdir of /etc/radius because it is declared
	# as a volume in the Dockerfile and this prevents it being covered by the parent bind mount
	docker run $(DOCKER_FLAGS) --rm -it -v=$(CURDIR)/radius:/etc/raddb -v=$(CURDIR)/radius/certs:/etc/raddb/certs -p 1812-1813:1812-1813/udp -p 3000:3000 -p 9812:9812 --name govwifi-frontend-c govwifi-frontend /usr/bin/run-local.sh

FORCE:
