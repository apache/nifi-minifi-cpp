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
UID_ARG=${UID_ARG:-1000}
GID_ARG=${GID_ARG:-1000}
MINIFI_VERSION=${MINIFI_VERSION:-}
IMAGE_TYPE=${IMAGE_TYPE:-release}
DUMP_LOCATION=${DUMP_LOCATION:-}
DISTRO_NAME=${DISTRO_NAME:-}
BUILD_NUMBER=${BUILD_NUMBER:-}
ENABLE_ALL=${ENABLE_ALL:-}
ENABLE_PYTHON=${ENABLE_PYTHON:-}
ENABLE_OPS=${ENABLE_OPS:-ON}
ENABLE_JNI=${ENABLE_JNI:-}
ENABLE_OPENCV=${ENABLE_OPENCV:-}
ENABLE_OPC=${ENABLE_OPC:-}
ENABLE_GPS=${ENABLE_GPS:-}
ENABLE_COAP=${ENABLE_COAP:-}
ENABLE_WEL=${ENABLE_WEL:-}
ENABLE_SQL=${ENABLE_SQL:-}
ENABLE_MQTT=${ENABLE_MQTT:-}
ENABLE_PCAP=${ENABLE_PCAP:-}
ENABLE_LIBRDKAFKA=${ENABLE_LIBRDKAFKA:-}
ENABLE_SENSORS=${ENABLE_SENSORS:-}
ENABLE_SQLITE=${ENABLE_SQLITE:-}
ENABLE_USB_CAMERA=${ENABLE_USB_CAMERA:-}
ENABLE_TENSORFLOW=${ENABLE_TENSORFLOW:-}
ENABLE_AWS=${ENABLE_AWS:-}
ENABLE_BUSTACHE=${ENABLE_BUSTACHE:-}
ENABLE_SFTP=${ENABLE_SFTP:-}
ENABLE_OPENWSMAN=${ENABLE_OPENWSMAN:-}
DISABLE_CURL=${DISABLE_CURL:-}
DISABLE_JEMALLOC=${DISABLE_JEMALLOC:-ON}
DISABLE_CIVET=${DISABLE_CIVET:-}
DISABLE_EXPRESSION_LANGUAGE=${DISABLE_EXPRESSION_LANGUAGE:-}
DISABLE_ROCKSDB=${DISABLE_ROCKSDB:-}
DISABLE_LIBARCHIVE=${DISABLE_LIBARCHIVE:-}
DISABLE_LZMA=${DISABLE_LZMA:-}
DISABLE_BZIP2=${DISABLE_BZIP2:-}
DISABLE_SCRIPTING=${DISABLE_SCRIPTING:-}
DISABLE_PYTHON_SCRIPTING=${DISABLE_PYTHON_SCRIPTING:-}
DISABLE_CONTROLLER=${DISABLE_CONTROLLER:-}
DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE:-}

function usage {
  echo "Usage: ./DockerBuild.sh -v <MINIFI_VERSION> [additional options]"
  echo "Options:"
  echo "-v, --minifi-version  Minifi version number to be used (required)"
  echo "-i, --image-type      Can be release or minimal (default: release)"
  echo "-u, --uid             User id to be used in the Docker image (default: 1000)"
  echo "-g, --gid             Group id to be used in the Docker image (default: 1000)"
  echo "-d, --distro-name     Linux distribution build to be used for alternative builds (xenial|bionic|fedora|debian|centos)"
  echo "-l  --dump-location   Path where to the output dump to be put"
  echo "-c  --cmake-param     CMake parameter passed in PARAM=value format"
  echo "-h  --help            Show this help message"
  exit 1
}

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
    -i|--image-type)
      IMAGE_TYPE="$2"
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
        declare "${ARR[0]}"="${ARR[1]}"
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
if [ "${IMAGE_TYPE}" != "release" ]; then
  TAG="${IMAGE_TYPE}-"
fi

TARGZ_TAG=""
if [ -n "${DISTRO_NAME}" ]; then
  TAG="${TAG}${DISTRO_NAME}-"
  TARGZ_TAG="${DISTRO_NAME}-"
fi

TAG="${TAG}${MINIFI_VERSION}"
TARGZ_TAG="${TARGZ_TAG}${MINIFI_VERSION}"

if [ -n "${BUILD_NUMBER}" ]; then
  TAG="${TAG}-${BUILD_NUMBER}"
  TARGZ_TAG="${TARGZ_TAG}-${BUILD_NUMBER}"
fi

DOCKER_COMMAND="docker build "
BUILD_ARGS="--build-arg UID=${UID_ARG} \
            --build-arg GID=${GID_ARG} \
            --build-arg MINIFI_VERSION=${MINIFI_VERSION} \
            --build-arg IMAGE_TYPE=${IMAGE_TYPE} \
            --build-arg DUMP_LOCATION=${DUMP_LOCATION} \
            --build-arg DISTRO_NAME=${DISTRO_NAME} \
            --build-arg ENABLE_ALL=${ENABLE_ALL} \
            --build-arg ENABLE_PYTHON=${ENABLE_PYTHON} \
            --build-arg ENABLE_OPS=${ENABLE_OPS} \
            --build-arg ENABLE_JNI=${ENABLE_JNI} \
            --build-arg ENABLE_OPENCV=${ENABLE_OPENCV} \
            --build-arg ENABLE_OPC=${ENABLE_OPC} \
            --build-arg ENABLE_GPS=${ENABLE_GPS} \
            --build-arg ENABLE_COAP=${ENABLE_COAP} \
            --build-arg ENABLE_WEL=${ENABLE_WEL} \
            --build-arg ENABLE_SQL=${ENABLE_SQL} \
            --build-arg ENABLE_MQTT=${ENABLE_MQTT} \
            --build-arg ENABLE_PCAP=${ENABLE_PCAP} \
            --build-arg ENABLE_LIBRDKAFKA=${ENABLE_LIBRDKAFKA} \
            --build-arg ENABLE_SENSORS=${ENABLE_SENSORS} \
            --build-arg ENABLE_SQLITE=${ENABLE_SQLITE} \
            --build-arg ENABLE_USB_CAMERA=${ENABLE_USB_CAMERA} \
            --build-arg ENABLE_TENSORFLOW=${ENABLE_TENSORFLOW} \
            --build-arg ENABLE_AWS=${ENABLE_AWS} \
            --build-arg ENABLE_BUSTACHE=${ENABLE_BUSTACHE} \
            --build-arg ENABLE_SFTP=${ENABLE_SFTP} \
            --build-arg ENABLE_OPENWSMAN=${ENABLE_OPENWSMAN} \
            --build-arg DISABLE_CURL=${DISABLE_CURL} \
            --build-arg DISABLE_JEMALLOC=${DISABLE_JEMALLOC} \
            --build-arg DISABLE_CIVET=${DISABLE_CIVET} \
            --build-arg DISABLE_EXPRESSION_LANGUAGE=${DISABLE_EXPRESSION_LANGUAGE} \
            --build-arg DISABLE_ROCKSDB=${DISABLE_ROCKSDB} \
            --build-arg DISABLE_LIBARCHIVE=${DISABLE_LIBARCHIVE} \
            --build-arg DISABLE_LZMA=${DISABLE_LZMA} \
            --build-arg DISABLE_BZIP2=${DISABLE_BZIP2} \
            --build-arg DISABLE_SCRIPTING=${DISABLE_SCRIPTING} \
            --build-arg DISABLE_PYTHON_SCRIPTING=${DISABLE_PYTHON_SCRIPTING} \
            --build-arg DISABLE_CONTROLLER=${DISABLE_CONTROLLER} "

if [ -n "${DOCKER_BASE_IMAGE}" ]; then
  BUILD_ARGS="${BUILD_ARGS} --build-arg BASE_ALPINE_IMAGE=${DOCKER_BASE_IMAGE}"
fi

DOCKER_COMMAND="${DOCKER_COMMAND} ${BUILD_ARGS} \
                --target ${IMAGE_TYPE} \
                -f ${DOCKERFILE} \
                -t \
                apacheminificpp:${TAG} .."

echo "Docker Command: '$DOCKER_COMMAND'"
DOCKER_BUILDKIT=1 ${DOCKER_COMMAND}

if [ -n "${DUMP_LOCATION}" ]; then
  docker run --rm --entrypoint cat "apacheminificpp:${TAG}" "/opt/minifi/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz" > "${DUMP_LOCATION}/nifi-minifi-cpp-${TARGZ_TAG}-bin.tar.gz"
fi
