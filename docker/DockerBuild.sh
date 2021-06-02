#!/bin/bash
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

# Fail on errors
set -euo pipefail

# Set env vars.
UID_ARG=1000
GID_ARG=1000
MINIFI_VERSION=
IMAGE_TAG=
DUMP_LOCATION=
DISTRO_NAME=
BUILD_NUMBER=

function usage {
  echo "Usage: ./DockerBuild.sh -v <MINIFI_VERSION> [additional options]"
  echo "Options:"
  echo "-v, --minifi-version  Minifi version number to be used (required)"
  echo "-t, --tag             Additional prefix added to the image tag"
  echo "-u, --uid             User id to be used in the Docker image (default: 1000)"
  echo "-g, --gid             Group id to be used in the Docker image (default: 1000)"
  echo "-d, --distro-name     Linux distribution build to be used for alternative builds (bionic|focal|fedora|debian|centos)"
  echo "-l  --dump-location   Path where to the output dump to be put"
  echo "-c  --cmake-param     CMake parameter passed in PARAM=value format"
  echo "-h  --help            Show this help message"
  exit 1
}

BUILD_ARGS=""
while [[ $# -gt 0 ]]; do
  key="$1"
  case $key in
    -u|--uid)
      UID_ARG="$2"
      shift
      shift
      ;;
    -g|--gid)
      GID_ARG="$2"
      shift
      shift
    ;;
    -v|--minifi-version)
    MINIFI_VERSION="$2"
    shift
    shift
    ;;
    -t|--tag)
      IMAGE_TAG="$2"
      shift
      shift
      ;;
    -d|--distro-name)
      DISTRO_NAME="$2"
      shift
      shift
      ;;
    -l|--dump-location)
      DUMP_LOCATION="$2"
      shift
      shift
      ;;
    -c|--cmake-param)
      IFS='=' read -ra ARR <<< "$2"
      if [[ ${#ARR[@]} -gt 1 ]]; then
        if [ "${ARR[0]}" == "BUILD_NUMBER" ]; then
          BUILD_NUMBER="${ARR[1]}"
        elif [ "${ARR[0]}" == "DOCKER_BASE_IMAGE" ]; then
          BUILD_ARGS="${BUILD_ARGS} --build-arg BASE_ALPINE_IMAGE=${ARR[1]}"
        else
          BUILD_ARGS="${BUILD_ARGS} --build-arg ${ARR[0]}=${ARR[1]}"
        fi
      fi
      shift
      shift
      ;;
    -h|--help)
      usage
      ;;
    *)
      echo "Unknown argument passed: $1"
      usage
      ;;
  esac
done

if [ -z "${MINIFI_VERSION}" ]; then
  usage
fi

echo "NiFi-MiNiFi-CPP Version: ${MINIFI_VERSION}"

if [ -n "${DISTRO_NAME}" ]; then
  DOCKERFILE="${DISTRO_NAME}/Dockerfile"
else
  DOCKERFILE="Dockerfile"
fi

TAG=""
if [ -n "${IMAGE_TAG}" ]; then
  TAG="${IMAGE_TAG}-"
fi
if [ -n "${DISTRO_NAME}" ]; then
  TAG="${TAG}${DISTRO_NAME}-"
fi
TAG="${TAG}${MINIFI_VERSION}"
if [ -n "${BUILD_NUMBER}" ]; then
  TAG="${TAG}-${BUILD_NUMBER}"
fi

TARGZ_TAG="bin"
if [ -n "${DISTRO_NAME}" ]; then
  TARGZ_TAG="${TARGZ_TAG}-${DISTRO_NAME}"
fi
if [ -n "${BUILD_NUMBER}" ]; then
  TARGZ_TAG="${TARGZ_TAG}-${BUILD_NUMBER}"
fi

DOCKER_COMMAND="docker build "
BUILD_ARGS="--build-arg UID=${UID_ARG} \
            --build-arg GID=${GID_ARG} \
            --build-arg MINIFI_VERSION=${MINIFI_VERSION} \
            --build-arg DUMP_LOCATION=${DUMP_LOCATION} \
            --build-arg DISTRO_NAME=${DISTRO_NAME} ${BUILD_ARGS}"

DOCKER_COMMAND="${DOCKER_COMMAND} ${BUILD_ARGS} \
                -f ${DOCKERFILE} \
                -t \
                apacheminificpp:${TAG} .."

echo "Docker Command: 'DOCKER_BUILDKIT=1 ${DOCKER_COMMAND}'"
DOCKER_BUILDKIT=1 ${DOCKER_COMMAND}

if [ -n "${DUMP_LOCATION}" ]; then
  docker run --rm --entrypoint cat "apacheminificpp:${TAG}" "/opt/minifi/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz" > "${DUMP_LOCATION}/nifi-minifi-cpp-${MINIFI_VERSION}-${TARGZ_TAG}.tar.gz"
fi
