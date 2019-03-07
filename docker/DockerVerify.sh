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
if [[ ! -d ./test-env-py3 ]]; then
  echo "Creating virtual environment in ./test-env-py3" 1>&2
  virtualenv --python=python3 ./test-env-py3
fi

echo "Activating virtual environment..." 1>&2
. ./test-env-py3/bin/activate
pip install --trusted-host pypi.python.org --upgrade pip setuptools

# Install test dependencies
echo "Installing test dependencies..." 1>&2

# hint include/library paths if homewbrew is in use
if brew list 2> /dev/null | grep openssl > /dev/null 2>&1; then
  echo "Using homebrew paths for openssl" 1>&2
  export LDFLAGS="-L$(brew --prefix openssl)/lib"
  export CFLAGS="-I$(brew --prefix openssl)/include"
  export SWIG_FEATURES="-cpperraswarn -includeall -I$(brew --prefix openssl)/include"
fi

pip install --upgrade \
            pytest \
            docker \
            PyYAML \
            m2crypto \
            watchdog
export JAVA_HOME="/usr/lib/jvm/default-jvm"
export PATH="$PATH:/usr/lib/jvm/default-jvm/bin"

export MINIFI_VERSION=0.6.0
export PYTHONPATH="${PYTHONPATH}:${docker_dir}/test/integration"

exec pytest -s -v "${docker_dir}"/test/integration
