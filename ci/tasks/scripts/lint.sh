#!/bin/bash

./src/ci/tasks/scripts/with-docker.sh

workspace_dir="${PWD}"
cd src || exit

make lint

cd "${workspace_dir}" || exit
