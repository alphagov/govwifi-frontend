# Variables

A list of variables and their descriptions used by these pipelines

## Secrets

These variables are secrets, provided by the hosted Concourse, or ones we have set up

### Github Access Token

Stored under: `github-access-token`

An access token for cloning public + private repos, and sending `status` updates to PRs.

This is associated with the `govwifi-jenkins` Github user account.

### Hosted ECR

Stored under: `readonly_private_ecr_repo_url`

This is a provided ECR instance, that we are able to access using the worker's IAM Role.

We can push to and read from the registry, however note that we must use tags to denote what the image
contains, as we are unable to set up new registries.

Only use for caching + passing images around jobs, do not use for deploying from.

We currently use this for holding our [`concourse-runner`][concourse-runner].




[concourse-runner]: https://github.com/alphagov/govwifi-concourse-runner
