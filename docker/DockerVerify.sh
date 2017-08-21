#!/bin/bash

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the \"License\"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an \"AS IS\" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

docker_dir="$( cd ${0%/*} && pwd )"

# Create virutal environment for testing
if [[ ! -d ./test-env-py2 ]]; then
  echo "Creating virtual environment in ./test-env-py2" 1>&2
  virtualenv ./test-env-py2
fi

echo "Activating virtual environment..." 1>&2
. ./test-env-py2/bin/activate
pip install --upgrade pip setuptools

# Install test dependencies
echo "Installing test dependencies..." 1>&2
pip install --upgrade pytest \
                      docker \
                      PyYAML \
                      m2crypto \
                      watchdog

export MINIFI_VERSION=0.3.0
export PYTHONPATH="${PYTHONPATH}:${docker_dir}/test/integration"

exec pytest -s -v "${docker_dir}"/test/integration
