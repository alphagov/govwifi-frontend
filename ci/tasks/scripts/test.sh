#!/bin/bash

./src/ci/tasks/scripts/with-docker.sh

workspace_dir="${PWD}"
cd src || exit

make test

cd "${workspace_dir}" || exit
