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

ARG BASE_IMAGE="alpine:3.19.1"

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
  ccache

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-${MINIFI_VERSION}
ENV MINIFI_VERSION ${MINIFI_VERSION}

# Setup minificpp user
RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER} && \
    install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR}
COPY --chown=${USER}:${USER} . ${MINIFI_BASE_DIR}

USER ${USER}

RUN mkdir ${MINIFI_BASE_DIR}/build
WORKDIR ${MINIFI_BASE_DIR}/build
RUN export PATH=/usr/lib64/ccache/bin${PATH:+:${PATH}} && \
    export CCACHE_DIR=${MINIFI_BASE_DIR}/.ccache && \
    cmake -DSTATIC_BUILD= -DSKIP_TESTS=${DOCKER_SKIP_TESTS} ${MINIFI_OPTIONS} -DAWS_ENABLE_UNITY_BUILD=OFF -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. && \
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

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME ${MINIFI_BASE_DIR}/minifi-current
ENV MINIFI_VERSIONED_HOME ${MINIFI_BASE_DIR}/nifi-minifi-cpp-${MINIFI_VERSION}

RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER} && \
    install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR} && ln -s ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME} && \
    apk add --no-cache libstdc++ tzdata alpine-conf && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_PYTHON_SCRIPTING=ON"; then apk add --no-cache python3; fi && \
    setup-timezone -z UTC && \
    apk del alpine-conf

# Copy built minifi distribution from builder
COPY --from=build --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME}
COPY --from=build --chown=${USER}:${USER} ${MINIFI_BASE_DIR}/docker/conf/minifi-log.properties ${MINIFI_HOME}/conf/minifi-log.properties

USER ${USER}
WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ["./bin/minifi.sh", "run"]
