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

ARG BASE_IMAGE="alpine:3.22.4"

# Build image
FROM ${BASE_IMAGE} AS build
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ARG MINIFI_VERSION
ARG UID=1000
ARG GID=1000

# MINIFI_OPTIONS will be passed directly to cmake
# use it to define cmake options (e.g. -DENABLE_AWS=ON -DENABLE_AZURE=ON)
ARG MINIFI_OPTIONS=""
ARG CMAKE_BUILD_TYPE=Release
ARG DOCKER_SKIP_TESTS=ON
ARG DOCKER_USE_CONAN=OFF
ARG DOCKER_NIFI_CONAN_USER=

# Install the system dependencies needed for a build
RUN apk --no-cache add gcc \
  g++ \
  make \
  bash \
  bison \
  flex \
  flex-dev \
  linux-headers \
  autoconf \
  automake \
  libtool \
  curl-dev \
  cmake \
  git \
  patch \
  python3-dev \
  doxygen \
  ccache \
  python3 \
  py3-pip \
  m4 \
  pkgconf

ENV USER=minificpp
ENV MINIFI_BASE_DIR=/opt/minifi
ENV MINIFI_HOME=$MINIFI_BASE_DIR/nifi-minifi-cpp-${MINIFI_VERSION}
ENV MINIFI_VERSION=${MINIFI_VERSION}

# Setup minificpp user
RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER} && \
    install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR}
COPY --chown=${USER}:${USER} . ${MINIFI_BASE_DIR}

USER ${USER}

RUN --mount=type=secret,id=nifi_conan_password,mode=0444 if [ "${DOCKER_USE_CONAN}" == "ON" ]; then \
      set -e; \
      cd ${MINIFI_BASE_DIR}/bootstrap && \
      python3 -m venv venv && \
      source venv/bin/activate && \
      pip install -r requirements.txt && \
      conan profile detect --exist-ok && \
      cp ${MINIFI_BASE_DIR}/docker/settings_user.yml "$(conan config home)/settings_user.yml" && \
      printf '\nos.libc=musl\n' >> "$(conan config home)/profiles/default" && \
      python3 main.py --skip-compiler-install --run-conan-install --minifi-options="${MINIFI_OPTIONS}" && \
      if [ -n "${DOCKER_NIFI_CONAN_USER}" ] && [ -f /run/secrets/nifi_conan_password ]; then \
        conan remote login nifi-conan ${DOCKER_NIFI_CONAN_USER} -p "$(cat /run/secrets/nifi_conan_password)" || true; \
        conan upload "*" -r nifi-conan --confirm || true; \
      fi; \
    fi

RUN mkdir -p ${MINIFI_BASE_DIR}/build
WORKDIR ${MINIFI_BASE_DIR}/build
RUN export PATH=/usr/lib64/ccache/bin${PATH:+:${PATH}} && \
    export CCACHE_DIR=${MINIFI_BASE_DIR}/.ccache && \
    if [ "${DOCKER_USE_CONAN}" == "ON" ]; then CONAN_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake"; fi && \
    cmake -DSTATIC_BUILD= -DSKIP_TESTS=${DOCKER_SKIP_TESTS} ${CONAN_TOOLCHAIN:-} ${MINIFI_OPTIONS} -DAWS_ENABLE_UNITY_BUILD=OFF -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. && \
    make -j "$(nproc)" package && \
    tar -xzvf "${MINIFI_BASE_DIR}/build/nifi-minifi-cpp-${MINIFI_VERSION}.tar.gz" -C "${MINIFI_BASE_DIR}"

# Release image
FROM ${BASE_IMAGE} AS release
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ARG UID=1000
ARG GID=1000
ARG MINIFI_VERSION

ARG MINIFI_OPTIONS

# Add testing repo for rocksdb
RUN echo 'http://dl-cdn.alpinelinux.org/alpine/edge/testing' >> /etc/apk/repositories

ENV USER=minificpp
ENV MINIFI_BASE_DIR=/opt/minifi
ENV MINIFI_HOME=${MINIFI_BASE_DIR}/minifi-current
ENV MINIFI_VERSIONED_HOME=${MINIFI_BASE_DIR}/nifi-minifi-cpp-${MINIFI_VERSION}

RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER} && \
    install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR} && ln -s ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME} && \
    apk add --no-cache libstdc++ tzdata alpine-conf && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_ALL=ON\|ENABLE_PYTHON_SCRIPTING=ON"; then apk add --no-cache python3; fi && \
    setup-timezone -z UTC && \
    apk del alpine-conf

# Copy built minifi distribution from builder
COPY --from=build --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME}
COPY --from=build --chown=${USER}:${USER} ${MINIFI_BASE_DIR}/docker/conf/minifi-log.properties ${MINIFI_HOME}/conf/minifi-log.properties

USER ${USER}
WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ["./bin/minifi.sh", "run"]
