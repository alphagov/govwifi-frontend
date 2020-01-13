# GovWifi Healtcheck

Performs healthchecks on GovWifi sessions.

## Overview

App is using a command line tool [eapol_test](https://www.mankier.com/8/eapol_test) to check health of RADIUS servers running on the same machine (localhost).

It can be trigger by a ping to `GET /` by load balancers or Route53 health checks.

All logs are sent to `STDOUT` which means, when running on EC2 instance, they will be saved automatically to CloudWatch.

### Sinatra routes

* `GET /` - Performs healthchecks and returns `200 OK` if all pass.

## Developing

### Running the tests

You can run  linter with the following command:

```shell
cd ..
make lint-healthcheck
```

### Serving the app locally

```shell
bundle exec rackup -o 0.0.0.0 -p 8080
```

Then access the site at [http://localhost:8080/](http://localhost:8080/)
