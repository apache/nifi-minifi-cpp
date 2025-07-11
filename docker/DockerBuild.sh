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
PREFIX=
DUMP_LOCATION=
DISTRO_NAME=
BUILD_NUMBER=
DOCKER_CCACHE_DUMP_LOCATION=
DOCKER_SKIP_TESTS=ON
CMAKE_BUILD_TYPE=Release
PLATFORMS=
PUSH_OR_LOAD="--load"
TAGS=()
MULTI_ARCH=no

function usage {
  echo "Usage: ./DockerBuild.sh -v <MINIFI_VERSION> [additional options]"
  echo "Options:"
  echo "-v, --minifi-version  Minifi version number to be used (required)"
  echo "-p, --prefix          Additional prefix added to the image tag"
  echo "-u, --uid             User id to be used in the Docker image (default: 1000)"
  echo "-g, --gid             Group id to be used in the Docker image (default: 1000)"
  echo "-d, --distro-name     Linux distribution build to be used for alternative builds (centos|rockylinux)"
  echo "-l  --dump-location   Path where to the output dump to be put"
  echo "-c  --cmake-param     CMake parameter passed in PARAM=value format"
  echo "-o  --options         Minifi options string"
  echo "-h  --help            Show this help message"
  exit 1
}

function dump_ccache() {
  ccache_source_image=$1
  docker_ccache_dump_location=$2
  container_id=$(docker create "${ccache_source_image}")
  mkdir -p "${docker_ccache_dump_location}"
  docker cp "${container_id}:/opt/minifi/.ccache/." "${docker_ccache_dump_location}"
  docker rm -f "${container_id}"
}

BUILD_ARGS=()
while [[ $# -gt 0 ]]; do
  key="$1"
  case $key in
  -u | --uid)
    UID_ARG="$2"
    shift
    shift
    ;;
  -g | --gid)
    GID_ARG="$2"
    shift
    shift
    ;;
  -v | --minifi-version)
    MINIFI_VERSION="$2"
    shift
    shift
    ;;
  -p | --prefix)
    PREFIX="$2"
    shift
    shift
    ;;
  -d | --distro-name)
    DISTRO_NAME="$2"
    shift
    shift
    ;;
  -l | --dump-location)
    DUMP_LOCATION="$2"
    shift
    shift
    ;;
  -c | --cmake-param)
    IFS='=' read -ra ARR <<<"$2"
    if [[ ${#ARR[@]} -gt 1 ]]; then
      if [ "${ARR[0]}" == "BUILD_NUMBER" ]; then
        BUILD_NUMBER="${ARR[1]}"
      elif [ "${ARR[0]}" == "DOCKER_BASE_IMAGE" ]; then
        BUILD_ARGS+=("--build-arg" "BASE_IMAGE=${ARR[1]}")
      elif [ "${ARR[0]}" == "DOCKER_CCACHE_DUMP_LOCATION" ]; then
        DOCKER_CCACHE_DUMP_LOCATION="${ARR[1]}"
      elif [ "${ARR[0]}" == "DOCKER_SKIP_TESTS" ]; then
        DOCKER_SKIP_TESTS="${ARR[1]}"
      elif [ "${ARR[0]}" == "CMAKE_BUILD_TYPE" ]; then
        CMAKE_BUILD_TYPE="${ARR[1]}"
      elif [ "${ARR[0]}" == "DOCKER_PLATFORMS" ] && [ -n "${ARR[1]}" ]; then
        PLATFORMS="--platform ${ARR[1]}"
      elif [ "${ARR[0]}" == "DOCKER_PUSH" ]; then
        if [ "${ARR[1]}" == "ON" ]; then
          PUSH_OR_LOAD="--push"
        fi
      elif [ "${ARR[0]}" == "DOCKER_TAGS" ] && [ -n "${ARR[1]}" ]; then
        IFS=', ' read -r -a TAGS <<< "${ARR[1]}"
      else
        BUILD_ARGS+=("--build-arg" "${ARR[0]}=${ARR[1]}")
      fi
    fi
    shift
    shift
    ;;
  -o | --options)
    BUILD_ARGS+=("--build-arg" "MINIFI_OPTIONS=$2")
    shift
    shift
    ;;
  -h | --help)
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

if [[ "${PLATFORMS}" == *,* ]]; then
  MULTI_ARCH=yes
fi

if { [ -n "${DUMP_LOCATION}" ] || [ -n "${DOCKER_CCACHE_DUMP_LOCATION}" ]; } && [ "${PUSH_OR_LOAD}" == "--push" ]; then
  echo "Error: Cannot save ccache dump or binaries locally when pushing image to remote registry. Turn DOCKER_PUSH flag OFF if you want to build binaries on docker."
  exit 1
fi

if { [ -n "${DUMP_LOCATION}" ] || [ -n "${DOCKER_CCACHE_DUMP_LOCATION}" ]; } && [ "${MULTI_ARCH}" == "yes" ]; then
  echo "Error: Cannot save ccache dump or binaries locally when using multi-arch image. Change DOCKER_PLATFORMS option to a single architecture if you want to build binaries on docker."
  exit 1
fi

if [ "${MULTI_ARCH}" == "yes" ] && [ "${PUSH_OR_LOAD}" == "--load" ]; then
  echo "Error: Multi-arch image cannot be locally built. Either change DOCKER_PLATFORMS flag to a single architecture or turn DOCKER_PUSH flag ON."
  exit 1
fi

echo "NiFi-MiNiFi-CPP Version: ${MINIFI_VERSION}"

if [ -n "${DISTRO_NAME}" ]; then
  DOCKERFILE="${DISTRO_NAME}/Dockerfile"
else
  DOCKERFILE="Dockerfile"
fi

if [ ${#TAGS[@]} -eq 0 ]; then
  TAG="apacheminificpp:"
  if [ -n "${PREFIX}" ]; then
    TAG="${TAG}${PREFIX}-"
  fi
  if [ -n "${DISTRO_NAME}" ]; then
    TAG="${TAG}${DISTRO_NAME}-"
  fi
  TAG="${TAG}${MINIFI_VERSION}"
  if [ -n "${BUILD_NUMBER}" ]; then
    TAG="${TAG}-${BUILD_NUMBER}"
  fi

  TAGS+=("${TAG}")

  TARGZ_TAG="bin"
  if [ -n "${DISTRO_NAME}" ]; then
    TARGZ_TAG="${TARGZ_TAG}-${DISTRO_NAME}"
  fi
  if [ -n "${BUILD_NUMBER}" ]; then
    TARGZ_TAG="${TARGZ_TAG}-${BUILD_NUMBER}"
  fi
fi

BUILD_ARGS+=("--build-arg" "UID=${UID_ARG}"
             "--build-arg" "GID=${GID_ARG}"
             "--build-arg" "MINIFI_VERSION=${MINIFI_VERSION}"
             "--build-arg" "DUMP_LOCATION=${DUMP_LOCATION}"
             "--build-arg" "DISTRO_NAME=${DISTRO_NAME}"
             "--build-arg" "DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}"
             "--build-arg" "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
if [ -n "${DUMP_LOCATION}" ]; then
  BUILD_ARGS+=("--build-arg" "DOCKER_MAKE_TARGET=package")
fi

TAGGING_CMD=""
for t in "${TAGS[@]}"; do
  TAGGING_CMD="${TAGGING_CMD} -t ${t}"
done

if [ -n "${DISTRO_NAME}" ]; then
  # shellcheck disable=SC2086
  echo docker buildx build "${BUILD_ARGS[@]}" -f "${DOCKERFILE}" ${PLATFORMS} ${PUSH_OR_LOAD} ${TAGGING_CMD} ..
  # shellcheck disable=SC2086
  docker buildx build "${BUILD_ARGS[@]}" -f "${DOCKERFILE}" ${PLATFORMS} ${PUSH_OR_LOAD} ${TAGGING_CMD} ..

  if [ -n "${DOCKER_CCACHE_DUMP_LOCATION}" ]; then
    dump_ccache "${TAG[0]}" "${DOCKER_CCACHE_DUMP_LOCATION}"
  fi
else
  if [ -n "${DOCKER_CCACHE_DUMP_LOCATION}" ]; then
    docker buildx build "${BUILD_ARGS[@]}" -f "${DOCKERFILE}" --target build -t minifi_build ..
    dump_ccache "minifi_build" "${DOCKER_CCACHE_DUMP_LOCATION}"
  fi
  # shellcheck disable=SC2086
  echo docker buildx build "${BUILD_ARGS[@]}" -f "${DOCKERFILE}" ${PLATFORMS} ${PUSH_OR_LOAD} ${TAGGING_CMD} ..
  # shellcheck disable=SC2086
  docker buildx build "${BUILD_ARGS[@]}" -f "${DOCKERFILE}" ${PLATFORMS} ${PUSH_OR_LOAD} ${TAGGING_CMD} ..
fi

if [ -n "${DUMP_LOCATION}" ]; then
  container_id=$(docker create "${TAG}")
  docker cp "${container_id}:/opt/minifi/build/nifi-minifi-cpp-${MINIFI_VERSION}.tar.gz" "${DUMP_LOCATION}/nifi-minifi-cpp-${MINIFI_VERSION}-${TARGZ_TAG}.tar.gz" || true
  docker cp "${container_id}:/opt/minifi/build/nifi-minifi-cpp-${MINIFI_VERSION}.rpm" "${DUMP_LOCATION}/nifi-minifi-cpp-${MINIFI_VERSION}-${TARGZ_TAG}.rpm" || true
  docker rm -f "${container_id}"
fi
