# GovWifi Frontend

## Purpose

This is the [FreeRADIUS][freeradius] configuration for the [GovWiFi][govwifi] project.

## How to install and use

Currently it is not possible to run this service from inside this repository alone.

We instead recommend using the [acceptance-tests][acceptance-tests] repo to set up a development environment for
making changes to FreeRADIUS or the healthcheck service.

Makefile targets are:

- `make test` - Currently a no-op. Tests are located in the [acceptance-tests][acceptance-tests] repo
- `make lint` - Runs linting on the healtcheck service, provided by `rubocop-govuk`

## Components

This project has three main components: the RADIUS server, the [FreeRADIUS Prometheus Exporter][prometheus-exporter], and the healthcheck service.

This RADIUS server is restarted daily by a separate app, the [Safe Restarter][safe-restarter].

### Healthcheck

The healthcheck service acts as an adapter to a monitoring service (Route53 Healthchecks).
When hit with a HTTP call, it will send a request to the radius server to ensure it can still
authorise users.

To accomplish this, [`eapol_test`][radius-testing] is used to simulate authentication using `PEAP-MSCHAPv2`.

All code is located under the `healthcheck` directory.

### Radius

FreeRadius is an implementation of the RADIUS protocol.

Our servers implement:

- EAP-TLS (client certificate authentication)
- PEAP-MSCHAPv2 (Protected EAP with username + password)

#### Files

There are currently 5 files fetched when the service is initialised.

- [clients.conf][freeradius-clients]
  Allows access points to communicate with the radius servers.
  This is generated by the [GovWifi Admin][govwifi-admin] service.
- ca.pem, server.pem, server.key, comodo.pem
  Used to set up TLS tunnels, and authenticate clients using EAP-TLS

They are currently stored in an encrypted S3 bucket, and only the RADIUS servers are authorised to access files within the bucket.

Files are fetched once a night when the servers are restarted for updates.

#### High Level Process

When someone attempts to use GovWifi:

1.  The username and password is sent to the radius server
2.  Radius receives, and sends a request to the [authentication backend][auth-backend] to fetch the known password
3.  The user password is checked against the known password
4.  the login attempt is logged in the [logging backend][logging-backend]
5.  either the user is accepted, or rejected depending on whether their password accepted.

### FreeRADIUS Prometheus Exporter

The [FreeRADIUS Prometheus Exporter][prometheus-exporter] is an open source Prometheus exporter for FreeRADIUS. 

It uses the [FreeRADIUS Status Server][freeradius-status-server] to query information about server state and the packages being processed. The Status Server is enabled by adding the `status` configuration file to the `radius/sites-enabled` directory.

The Prometheus exporter exposes these metrics on `/metrics` which can be then read by a Prometheus server.

For more information see the FreeRADIUS Prometheus Exporter's [readme][prometheus-exporter]. For information about configuring the Status Server please see FreeRADIUS's [documentation][freeradius-status-server-config].

## How to contribute

1.  Fork the project
2.  Create a feature or fix branch
3.  Run the linter: `make lint`
4.  Run the [acceptance tests][acceptance-tests]
5.  Raise a pull request

## License

This codebase is released under [the MIT Licence][mit].
                                                                
[mit]: LICENCE
[govwifi]: https://www.gov.uk/government/publications/govwifi/govwifi
[freeradius]: https://freeradius.org/
[govwifi-build]: https://github.com/alphagov/govwifi-build
[acceptance-tests]: https://github.com/alphagov/govwifi-acceptance-tests
[radius-testing]: https://wiki.freeradius.org/guide/eduroam#testing
[govwifi-admin]: https://admin.wifi.service.gov.uk
[freeradius-clients]: https://github.com/FreeRADIUS/freeradius-server/blob/v3.0.x/raddb/clients.conf
[auth-backend]: https://github.com/alphagov/govwifi-authentication-api
[logging-backend]: https://github.com/alphagov/govwifi-logging-api
[safe-restarter]: https://github.com/alphagov/govwifi-safe-restarter 
[prometheus-exporter]: https://github.com/bvantagelimited/freeradius_exporter
[freeradius-status-server]: https://wiki.freeradius.org/config/Status
[freeradius-status-server-config]: https://wiki.freeradius.org/config/Status#configuration
