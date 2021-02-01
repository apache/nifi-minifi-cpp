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

if [[ $# -lt 1 ]]; then
  echo "Usage:"
  echo "  ./DockerVerify.sh <MINIFI_VERSION>"
  exit 1
fi

docker_dir="$( cd "${0%/*}" && pwd )"

export MINIFI_VERSION=$1

# Create virutal environment for testing
if [[ ! -d ./test-env-py3 ]]; then
  echo "Creating virtual environment in ./test-env-py3" 1>&2
  virtualenv --python=python3 ./test-env-py3
fi

echo "Activating virtual environment..." 1>&2
# shellcheck disable=SC1091
. ./test-env-py3/bin/activate
pip install --trusted-host pypi.python.org --upgrade pip setuptools

# Install test dependencies
echo "Installing test dependencies..." 1>&2

# hint include/library paths if homewbrew is in use
if brew list 2> /dev/null | grep openssl > /dev/null 2>&1; then
  echo "Using homebrew paths for openssl" 1>&2
  LDFLAGS="-L$(brew --prefix openssl)/lib"
  export LDFLAGS
  CFLAGS="-I$(brew --prefix openssl)/include"
  export CFLAGS
  SWIG_FEATURES="-cpperraswarn -includeall -I$(brew --prefix openssl)/include"
  export SWIG_FEATURES
fi

if ! command swig -version &> /dev/null; then
  echo "Swig could not be found on your system (dependency of m2crypto python library). Please install swig to continue."
  exit 1
fi

pip install --upgrade \
            behave \
            pytest \
            pytimeparse \
            docker \
            PyYAML \
            m2crypto \
            watchdog
JAVA_HOME="/usr/lib/jvm/default-jvm"
export JAVA_HOME
PATH="$PATH:/usr/lib/jvm/default-jvm/bin"
export PATH

PYTHONPATH="${PYTHONPATH}:${docker_dir}/test/integration"
export PYTHONPATH

# exec behave -f pretty "${docker_dir}/test/integration/features/processors.feature"
cd "${docker_dir}/test/integration"
exec 
  # behave -f pretty --logging-level WARNING --capture "features/file_system_operations.feature" &&
  # behave -f pretty --logging-level INFO --no-capture "features/s2s.feature" &&
  # behave -f pretty --logging-level INFO --no-capture "features/http.feature"
  behave -f pretty --logging-level INFO --no-capture "features/http.feature" -n "A MiNiFi instance sends data to a HTTP proxy and another one listens"
# exec pytest --log-cli-level=10 --log-level=INFO -v "${docker_dir}"/test/integration/test_filesystem_ops.py
# exec pytest --log-cli-level=10 --log-level=INFO -v "${docker_dir}"/test/integration/
