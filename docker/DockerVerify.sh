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
            pytimeparse \
            docker \
            pykafka \
            kafka-python \
            confluent-kafka \
            PyYAML \
            m2crypto \
            watchdog
JAVA_HOME="/usr/lib/jvm/default-jvm"
export JAVA_HOME
PATH="$PATH:/usr/lib/jvm/default-jvm/bin"
export PATH

PYTHONPATH="${PYTHONPATH}:${docker_dir}/test/integration"
export PYTHONPATH

BEHAVE_OPTS=(-f pretty --logging-level INFO --logging-clear-handlers)

cd "${docker_dir}/test/integration"
exec
  behave "${BEHAVE_OPTS[@]}" "features/file_system_operations.feature" -n "Get and put operations run in a simple flow" &&
  behave "${BEHAVE_OPTS[@]}" "features/file_system_operations.feature" -n "PutFile does not overwrite a file that already exists" &&
  behave "${BEHAVE_OPTS[@]}" "features/s2s.feature" -n "A MiNiFi instance produces and transfers data to a NiFi instance via s2s" &&
  behave "${BEHAVE_OPTS[@]}" "features/s2s.feature" -n "Zero length files are transfered between via s2s if the \"drop empty\" connection property is false" &&
  behave "${BEHAVE_OPTS[@]}" "features/s2s.feature" -n "Zero length files are not transfered between via s2s if the \"drop empty\" connection property is true" &&
  behave "${BEHAVE_OPTS[@]}" "features/http.feature" -n "A MiNiFi instance transfers data to another MiNiFi instance" &&
  behave "${BEHAVE_OPTS[@]}" "features/http.feature" -n "A MiNiFi instance sends data through a HTTP proxy and another one listens" &&
  behave "${BEHAVE_OPTS[@]}" "features/http.feature" -n "A MiNiFi instance and transfers hashed data to another MiNiFi instance" &&
  behave "${BEHAVE_OPTS[@]}" "features/http.feature" -n "A MiNiFi instance transfers data to another MiNiFi instance without message body" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "A MiNiFi instance transfers data to a kafka broker" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "PublishKafka sends flowfiles to failure when the broker is not available" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "PublishKafka sends can use SSL connect" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "MiNiFi consumes data from a kafka topic" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "ConsumeKafka parses and uses kafka topics and topic name formats" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "ConsumeKafka key attribute is encoded according to the \"Key Attribute Encoding\" property" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "Headers on consumed kafka messages are extracted into attributes if requested on ConsumeKafka" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "ConsumeKafka transactional behaviour is supported" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "Key attribute is encoded according to the \"Key Attribute Encoding\" property" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "Messages are separated into multiple flowfiles if the message demarcator is present in the message" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "The ConsumeKafka \"Maximum Poll Records\" property sets a limit on the messages processed in a single batch" &&
  behave "${BEHAVE_OPTS[@]}" "features/kafka.feature" -n "Unsupported encoding attributes for ConsumeKafka throw scheduling errors" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "A MiNiFi instance transfers encoded data to s3" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "A MiNiFi instance transfers encoded data through a http proxy to s3" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "A MiNiFi instance can remove s3 bucket objects" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "Deletion of a s3 object through a proxy-server succeeds" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "A MiNiFi instance can download s3 bucket objects directly" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "A MiNiFi instance can download s3 bucket objects via a http-proxy" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "A MiNiFi instance can list an S3 bucket directly" &&
  behave "${BEHAVE_OPTS[@]}" "features/s3.feature" -n "A MiNiFi instance can list an S3 bucket objects via a http-proxy" &&
  behave "${BEHAVE_OPTS[@]}" "features/azure_storage.feature" -n "A MiNiFi instance can upload data to Azure blob storage"
