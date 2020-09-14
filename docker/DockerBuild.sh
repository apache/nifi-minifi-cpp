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

# Fail on errors
set -e

# Set env vars.
UID_ARG=$1
GID_ARG=$2
MINIFI_VERSION=$3
IMAGE_TYPE=${4:-release}
ENABLE_JNI=${5:-}
DUMP_LOCATION=${6:-}
DISTRO_NAME=${7:-}

echo "NiFi-MiNiFi-CPP Version: ${MINIFI_VERSION}"

if [ -n "${DISTRO_NAME}" ]; then
  DOCKERFILE="${DISTRO_NAME}/Dockerfile"
else
  DOCKERFILE="Dockerfile"
fi

unset TAG
if [ "${IMAGE_TYPE}" != "release" ]; then
  TAG="${IMAGE_TYPE}-"
fi

if [ -n "${DISTRO_NAME}" ]; then
  TAG="${TAG}${DISTRO_NAME}-"
fi

TAG="${TAG}${MINIFI_VERSION}"

DOCKER_COMMAND="docker build --build-arg UID=${UID_ARG} \
                             --build-arg GID=${GID_ARG} \
                             --build-arg MINIFI_VERSION=${MINIFI_VERSION} \
                             --build-arg ENABLE_JNI=${ENABLE_JNI} \
                             --target ${IMAGE_TYPE} \
                             -f ${DOCKERFILE} \
                             -t \
                             apacheminificpp:${TAG} .."
echo "Docker Command: '$DOCKER_COMMAND'"
${DOCKER_COMMAND}

if [ -n "${DUMP_LOCATION}" ]; then
  docker run --rm --entrypoint cat "apacheminificpp:${TAG}" "/opt/minifi/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz" > "${DUMP_LOCATION}/nifi-minifi-cpp-${DISTRO_NAME}-${MINIFI_VERSION}-bin.tar.gz"
fi
