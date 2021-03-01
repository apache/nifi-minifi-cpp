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

# First stage: the common build environment dependencies
ARG BASE_ALPINE_IMAGE="alpine:3.12"
FROM ${BASE_ALPINE_IMAGE} AS build_deps
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ARG MINIFI_VERSION
ARG UID=1000
ARG GID=1000

# Install the system dependencies needed for a build
RUN apk --update --no-cache upgrade && apk --update --no-cache add gcc \
  g++ \
  make \
  bison \
  flex \
  flex-dev \
  maven \
  openjdk8-jre-base \
  openjdk8 \
  autoconf \
  automake \
  libtool \
  wget \
  gdb \
  musl-dev \
  vim \
  util-linux-dev \
  curl-dev \
  cmake \
  git \
  nss \
  nss-dev \
  unzip \
  gpsd-dev \
  libressl-dev \
  zlib-dev \
  bzip2-dev \
  python3-dev \
  patch \
  doxygen

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV JAVA_HOME /usr/lib/jvm/default-jvm
ENV PATH ${PATH}:/usr/lib/jvm/default-jvm/bin
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-${MINIFI_VERSION}
ENV MINIFI_VERSION ${MINIFI_VERSION}

# Setup minificpp user
RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER}

RUN install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR}
COPY --chown=${USER}:${USER} . ${MINIFI_BASE_DIR}

USER ${USER}


# Build stage of the minimal image
FROM build_deps AS build_minimal
RUN cd ${MINIFI_BASE_DIR} \
  && mkdir build \
  && cd build \
  && cmake -DENABLE_PYTHON=OFF -DDISABLE_SCRIPTING=ON -DENABLE_LIBRDKAFKA=ON -DENABLE_AWS=ON -DSKIP_TESTS=true -DCMAKE_BUILD_TYPE=MinSizeRel .. \
  && make -j$(nproc) package \
  && tar -xzvf ${MINIFI_BASE_DIR}/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz -C ${MINIFI_BASE_DIR}


# Build stage of normal image
FROM build_deps AS build_release
ARG ENABLE_ALL
ARG ENABLE_PYTHON
ARG ENABLE_OPS
ARG ENABLE_JNI
ARG ENABLE_OPENCV
ARG ENABLE_OPC
ARG ENABLE_GPS
ARG ENABLE_COAP
ARG ENABLE_WEL
ARG ENABLE_SQL
ARG ENABLE_MQTT
ARG ENABLE_PCAP
ARG ENABLE_LIBRDKAFKA
ARG ENABLE_SENSORS
ARG ENABLE_SQLITE
ARG ENABLE_USB_CAMERA
ARG ENABLE_TENSORFLOW
ARG ENABLE_AWS
ARG ENABLE_BUSTACHE
ARG ENABLE_SFTP
ARG ENABLE_OPENWSMAN
ARG DISABLE_CURL
ARG DISABLE_JEMALLOC
ARG DISABLE_CIVET
ARG DISABLE_EXPRESSION_LANGUAGE
ARG DISABLE_ROCKSDB
ARG DISABLE_LIBARCHIVE
ARG DISABLE_LZMA
ARG DISABLE_BZIP2
ARG DISABLE_SCRIPTING
ARG DISABLE_PYTHON_SCRIPTING
ARG DISABLE_CONTROLLER
RUN cd ${MINIFI_BASE_DIR} \
  && mkdir build \
  && cd build \
  && cmake -DSTATIC_BUILD= -DSKIP_TESTS=true -DENABLE_ALL=${ENABLE_ALL} -DENABLE_PYTHON=${ENABLE_PYTHON} -DENABLE_OPS=${ENABLE_OPS} \
    -DENABLE_JNI=${ENABLE_JNI} -DENABLE_OPENCV=${ENABLE_OPENCV} -DENABLE_OPC=${ENABLE_OPC} -DENABLE_GPS=${ENABLE_GPS} -DENABLE_COAP=${ENABLE_COAP} \
    -DENABLE_WEL=${ENABLE_WEL} -DENABLE_SQL=${ENABLE_SQL} -DENABLE_MQTT=${ENABLE_MQTT} -DENABLE_PCAP=${ENABLE_PCAP} \
    -DENABLE_LIBRDKAFKA=${ENABLE_LIBRDKAFKA} -DENABLE_SENSORS=${ENABLE_SENSORS} -DENABLE_SQLITE=${ENABLE_SQLITE} \
    -DENABLE_USB_CAMERA=${ENABLE_USB_CAMERA} -DENABLE_TENSORFLOW=${ENABLE_TENSORFLOW} -DENABLE_AWS=${ENABLE_AWS} \
    -DENABLE_BUSTACHE=${ENABLE_BUSTACHE} -DENABLE_SFTP=${ENABLE_SFTP} -DENABLE_OPENWSMAN=${ENABLE_OPENWSMAN} \
    -DDISABLE_CURL=${DISABLE_CURL} -DDISABLE_JEMALLOC=${DISABLE_JEMALLOC} -DDISABLE_CIVET=${DISABLE_CIVET} \
    -DDISABLE_EXPRESSION_LANGUAGE=${DISABLE_EXPRESSION_LANGUAGE} -DDISABLE_ROCKSDB=${DISABLE_ROCKSDB} \
    -DDISABLE_LIBARCHIVE=${DISABLE_LIBARCHIVE} -DDISABLE_LZMA=${DISABLE_LZMA} -DDISABLE_BZIP2=${DISABLE_BZIP2} \
    -DDISABLE_SCRIPTING=${DISABLE_SCRIPTING} -DDISABLE_PYTHON_SCRIPTING=${DDISABLE_PYTHON_SCRIPTING} -DDISABLE_CONTROLLER=${DISABLE_CONTROLLER} -DCMAKE_BUILD_TYPE=Release .. \
  && make -j$(nproc) package \
  && tar -xzvf ${MINIFI_BASE_DIR}/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz -C ${MINIFI_BASE_DIR}


# Common runtime image dependencies
# Edge required for rocksdb
FROM ${BASE_ALPINE_IMAGE} AS common_runtime_deps

ARG UID=1000
ARG GID=1000
ARG MINIFI_VERSION

# Add testing repo for rocksdb
RUN echo 'http://dl-cdn.alpinelinux.org/alpine/edge/testing' >> /etc/apk/repositories

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME ${MINIFI_BASE_DIR}/minifi-current
ENV MINIFI_VERSIONED_HOME ${MINIFI_BASE_DIR}/nifi-minifi-cpp-${MINIFI_VERSION}
ENV JAVA_HOME /usr/lib/jvm/default-jvm
ENV PATH ${PATH}:/usr/lib/jvm/default-jvm/bin

RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER}
RUN install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR} \
  && ln -s ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME}


# Final stage of the minimal image
FROM common_runtime_deps AS minimal

RUN apk --update --no-cache upgrade && apk add --update --no-cache libstdc++
RUN install -d -o ${USER} -g ${USER} ${MINIFI_VERSIONED_HOME}/bin \
  && install -d -o ${USER} -g ${USER} ${MINIFI_VERSIONED_HOME}/conf && chown ${USER}:${USER} ${MINIFI_HOME}

# Copy built minifi distribution from builder
COPY --from=build_minimal --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME}/bin/minifi ${MINIFI_HOME}/bin/minifi
COPY --from=build_minimal --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME}/bin/minifi.sh ${MINIFI_HOME}/bin/minifi.sh
COPY --from=build_minimal --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME}/conf ${MINIFI_HOME}/conf

USER ${USER}
WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ./bin/minifi.sh run


# Final stage of release image
FROM common_runtime_deps AS release
RUN apk --update --no-cache upgrade && apk add --update --no-cache \
  util-linux \
  curl \
  unzip \
  gpsd \
  openjdk8-jre-base \
  openjdk8 \
  nss \
  nss-dev \
  libressl \
  python3 \
  zlib

# Copy built minifi distribution from builder
COPY --from=build_release --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME}

USER ${USER}
WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ./bin/minifi.sh run
