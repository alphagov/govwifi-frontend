

DOCKER_COMPOSE = docker-compose -f docker-compose.yml

ifdef ON_CONCOURSE
  DOCKER_COMPOSE += -f docker-compose.concourse.yml
endif

ifndef JENKINS_URL
  ifndef ON_CONCOURSE
    DOCKER_COMPOSE += -f docker-compose.development.yml
  endif
endif

RUN_APP = ${DOCKER_COMPOSE} run --rm app
RUN_APP_PORTS = ${DOCKER_COMPOSE} run --rm --service-ports app


build: docker-build

prebuild: docker-prebuild

lint: lint-healthcheck lint-radius

test: test-healthcheck test-radius

.PHONY: build lint test

## healthcheck related tasks

lint-healthcheck: build
	${RUN_APP} bundle exec govuk-lint-ruby

test-healthcheck:
	@echo "no healthcheck tests yet"

.PHONY: lint-healthcheck test-healthcheck

## Radius related tasks

lint-radius:
	@echo "no radius linting yet"

test-radius:
	@echo "no radius tests yet"

.PHONY: lint-radius test-radius

## Docker specific tasks

docker-build:
ifndef ON_CONCOURSE
	${DOCKER_COMPOSE} build
endif

docker-prebuild:
	${DOCKER_COMPOSE} build
	${DOCKER_COMPOSE} up --no-start

.PHONY: docker-build docker-prebuild
