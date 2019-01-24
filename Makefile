

DOCKER_COMPOSE = docker-compose
RUN_APP = ${DOCKER_COMPOSE} run --rm app
RUN_APP_PORTS = ${DOCKER_COMPOSE} run --rm --service-ports app

build: docker-build

lint: build
	${RUN_APP} bundle exec govuk-lint-ruby

test: build
	@echo "no tests yet"

## Docker specific tasks

docker-build:
	${DOCKER_COMPOSE} build


.PHONY: build lint test
.PHONY: docker-build
