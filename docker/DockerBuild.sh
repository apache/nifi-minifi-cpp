# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements. See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership. The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the License for the
# specific language governing permissions and limitations
# under the License.
#

#!/bin/bash

# Set env vars.
UID_ARG=$1
GID_ARG=$2
MINIFI_VERSION=$3
MINIFI_SOURCE_CODE=$4
CMAKE_SOURCE_DIR=$5

echo "NiFi-MiNiFi-CPP Version: $MINIFI_VERSION"
echo "Current Working Directory: $(pwd)"
echo "CMake Source Directory: $CMAKE_SOURCE_DIR"
echo "MiNiFi Package: $MINIFI_SOURCE_CODE"

# Copy the MiNiFi source tree to the Docker working directory before building
mkdir -p $CMAKE_SOURCE_DIR/docker/minificppsource
rsync -avr \
      --exclude '/*build*' \
      --exclude '/docker' \
      --exclude '.git' \
      --exclude '/extensions/expression-language/Parser.cpp' \
      --exclude '/extensions/expression-language/Parser.hpp' \
      --exclude '/extensions/expression-language/Scanner.cpp' \
      --exclude '/extensions/expression-language/location.hh' \
      --exclude '/extensions/expression-language/position.hh' \
      --exclude '/extensions/expression-language/stack.hh' \
      --delete \
      $CMAKE_SOURCE_DIR/ \
      $CMAKE_SOURCE_DIR/docker/minificppsource/

DOCKER_COMMAND="docker build --build-arg UID=$UID_ARG \
                             --build-arg GID=$GID_ARG \
                             --build-arg MINIFI_VERSION=$MINIFI_VERSION \
                             --build-arg MINIFI_SOURCE_CODE=$MINIFI_SOURCE_CODE \
                             -t \
                             apacheminificpp:$MINIFI_VERSION ."
echo "Docker Command: '$DOCKER_COMMAND'"
${DOCKER_COMMAND}
