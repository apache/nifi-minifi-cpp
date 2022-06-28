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

ARG BASE_ALPINE_IMAGE="alpine:3.13"

# Build image
FROM ${BASE_ALPINE_IMAGE} AS build
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ARG MINIFI_VERSION
ARG UID=1000
ARG GID=1000

# PDH and WEL extensions and not listed as they are Windows specific
# SYSTEMD extension is turned OFF explicitly as it has no use in an alpine container
ARG ENABLE_ALL=OFF
ARG ENABLE_PYTHON=OFF
ARG ENABLE_OPS=ON
ARG ENABLE_JNI=OFF
ARG ENABLE_OPENCV=OFF
ARG ENABLE_OPC=OFF
ARG ENABLE_GPS=OFF
ARG ENABLE_COAP=OFF
ARG ENABLE_SQL=OFF
ARG ENABLE_MQTT=OFF
ARG ENABLE_PCAP=OFF
ARG ENABLE_LIBRDKAFKA=OFF
ARG ENABLE_SENSORS=OFF
ARG ENABLE_USB_CAMERA=OFF
ARG ENABLE_TENSORFLOW=OFF
ARG ENABLE_AWS=OFF
ARG ENABLE_BUSTACHE=OFF
ARG ENABLE_SFTP=OFF
ARG ENABLE_OPENWSMAN=OFF
ARG ENABLE_AZURE=OFF
ARG ENABLE_ENCRYPT_CONFIG=ON
ARG ENABLE_NANOFI=OFF
ARG ENABLE_SPLUNK=OFF
ARG ENABLE_GCP=OFF
ARG ENABLE_ELASTICSEARCH=OFF
ARG ENABLE_TEST_PROCESSORS=OFF
ARG DISABLE_CURL=OFF
ARG DISABLE_JEMALLOC=ON
ARG DISABLE_CIVET=OFF
ARG DISABLE_EXPRESSION_LANGUAGE=OFF
ARG DISABLE_ROCKSDB=OFF
ARG DISABLE_LIBARCHIVE=OFF
ARG DISABLE_LZMA=OFF
ARG DISABLE_BZIP2=OFF
ARG ENABLE_SCRIPTING=OFF
ARG DISABLE_PYTHON_SCRIPTING=
ARG ENABLE_LUA_SCRIPTING=
ARG ENABLE_KUBERNETES=OFF
ARG ENABLE_PROCFS=OFF
ARG ENABLE_PROMETHEUS=OFF
ARG DISABLE_CONTROLLER=OFF
ARG CMAKE_BUILD_TYPE=Release

# Install the system dependencies needed for a build
RUN apk --no-cache add gcc \
  g++ \
  make \
  bison \
  flex \
  flex-dev \
  linux-headers \
  maven \
  openjdk8-jre-base \
  openjdk8 \
  autoconf \
  automake \
  libtool \
  curl-dev \
  cmake \
  git \
  patch \
  libpcap-dev \
  libpng-dev \
  libusb-dev \
  gpsd-dev \
  python3-dev \
  boost-dev \
  doxygen \
  ccache \
  lua-dev

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV JAVA_HOME /usr/lib/jvm/default-jvm
ENV PATH ${PATH}:/usr/lib/jvm/default-jvm/bin
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-${MINIFI_VERSION}
ENV MINIFI_VERSION ${MINIFI_VERSION}

# Setup minificpp user
RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER} && \
    install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR}
COPY --chown=${USER}:${USER} . ${MINIFI_BASE_DIR}
RUN if [ -d "${MINIFI_BASE_DIR}/.ccache" ]; then mv ${MINIFI_BASE_DIR}/.ccache /home/${USER}/.ccache; fi

USER ${USER}

ENV PATH /usr/lib/ccache/bin:${PATH}
RUN mkdir ${MINIFI_BASE_DIR}/build
WORKDIR ${MINIFI_BASE_DIR}/build
RUN cmake -DSTATIC_BUILD= -DSKIP_TESTS=true -DENABLE_ALL="${ENABLE_ALL}" -DENABLE_PYTHON="${ENABLE_PYTHON}" -DENABLE_OPS="${ENABLE_OPS}" \
    -DENABLE_JNI="${ENABLE_JNI}" -DENABLE_OPENCV="${ENABLE_OPENCV}" -DENABLE_OPC="${ENABLE_OPC}" -DENABLE_GPS="${ENABLE_GPS}" \
    -DENABLE_COAP="${ENABLE_COAP}" -DENABLE_SQL="${ENABLE_SQL}" -DENABLE_MQTT="${ENABLE_MQTT}" -DENABLE_PCAP="${ENABLE_PCAP}" \
    -DENABLE_LIBRDKAFKA="${ENABLE_LIBRDKAFKA}" -DENABLE_SENSORS="${ENABLE_SENSORS}" -DENABLE_USB_CAMERA="${ENABLE_USB_CAMERA}" \
    -DENABLE_TENSORFLOW="${ENABLE_TENSORFLOW}" -DENABLE_AWS="${ENABLE_AWS}" -DENABLE_BUSTACHE="${ENABLE_BUSTACHE}" -DENABLE_SFTP="${ENABLE_SFTP}" \
    -DENABLE_OPENWSMAN="${ENABLE_OPENWSMAN}" -DENABLE_AZURE="${ENABLE_AZURE}" -DENABLE_NANOFI=${ENABLE_NANOFI} -DENABLE_SYSTEMD=OFF \
    -DDISABLE_CURL="${DISABLE_CURL}" -DDISABLE_JEMALLOC="${DISABLE_JEMALLOC}" -DDISABLE_CIVET="${DISABLE_CIVET}" -DENABLE_SPLUNK=${ENABLE_SPLUNK} \
    -DENABLE_TEST_PROCESSORS="${ENABLE_TEST_PROCESSORS}" -DDISABLE_EXPRESSION_LANGUAGE="${DISABLE_EXPRESSION_LANGUAGE}" -DDISABLE_ROCKSDB="${DISABLE_ROCKSDB}" \
    -DDISABLE_LIBARCHIVE="${DISABLE_LIBARCHIVE}" -DDISABLE_LZMA="${DISABLE_LZMA}" -DDISABLE_BZIP2="${DISABLE_BZIP2}" \
    -DENABLE_SCRIPTING="${ENABLE_SCRIPTING}" -DDISABLE_PYTHON_SCRIPTING="${DISABLE_PYTHON_SCRIPTING}" -DENABLE_LUA_SCRIPTING="${ENABLE_LUA_SCRIPTING}" \
    -DENABLE_KUBERNETES="${ENABLE_KUBERNETES}" -DENABLE_GCP="${ENABLE_GCP}" -DENABLE_PROCFS="${ENABLE_PROCFS}" -DENABLE_PROMETHEUS="${ENABLE_PROMETHEUS}" \
    -DENABLE_ELASTICSEARCH="${ENABLE_ELASTICSEARCH}" -DDISABLE_CONTROLLER="${DISABLE_CONTROLLER}" -DENABLE_ENCRYPT_CONFIG="${ENABLE_ENCRYPT_CONFIG}" \
    -DAWS_ENABLE_UNITY_BUILD=OFF -DEXCLUDE_BOOST=ON -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. && \
    make -j "$(nproc)" package && \
    tar -xzvf "${MINIFI_BASE_DIR}/build/nifi-minifi-cpp-${MINIFI_VERSION}.tar.gz" -C "${MINIFI_BASE_DIR}"


# Release image
FROM ${BASE_ALPINE_IMAGE} AS release
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ARG UID=1000
ARG GID=1000
ARG MINIFI_VERSION

ARG ENABLE_GPS=OFF
ARG ENABLE_JNI=OFF
ARG ENABLE_PCAP=OFF
ARG ENABLE_USB_CAMERA=OFF
ARG ENABLE_OPENCV=OFF
ARG ENABLE_PYTHON=OFF
ARG ENABLE_BUSTACHE=OFF
ARG ENABLE_SCRIPTING=OFF
ARG DISABLE_PYTHON_SCRIPTING=
ARG ENABLE_LUA_SCRIPTING=
ARG ENABLE_KUBERNETES=OFF

# Add testing repo for rocksdb
RUN echo 'http://dl-cdn.alpinelinux.org/alpine/edge/testing' >> /etc/apk/repositories

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME ${MINIFI_BASE_DIR}/minifi-current
ENV MINIFI_VERSIONED_HOME ${MINIFI_BASE_DIR}/nifi-minifi-cpp-${MINIFI_VERSION}
ENV JAVA_HOME /usr/lib/jvm/default-jvm
ENV PATH ${PATH}:/usr/lib/jvm/default-jvm/bin

RUN addgroup -g ${GID} ${USER} && adduser -u ${UID} -D -G ${USER} -g "" ${USER} && \
    install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR} && ln -s ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME} && \
    apk add --no-cache libstdc++ tzdata alpine-conf && \
    if [ "$ENABLE_GPS" = "ON" ]; then apk add --no-cache gpsd; fi && \
    if [ "$ENABLE_JNI" = "ON" ]; then apk add --no-cache openjdk8-jre-base; fi && \
    if [ "$ENABLE_PCAP" = "ON" ]; then apk add --no-cache libpcap; fi && \
    if [ "$ENABLE_USB_CAMERA" = "ON" ]; then apk add --no-cache libpng libusb; fi && \
    if [ "$ENABLE_OPENCV" = "ON" ] || [ "$ENABLE_BUSTACHE" = "ON" ]; then apk add --no-cache boost; fi && \
    if [ "$ENABLE_SCRIPTING" = "ON" ] && [ -n "$ENABLE_LUA_SCRIPTING" ]; then apk add --no-cache lua; fi && \
    if [ "$ENABLE_SCRIPTING" = "ON" ] && [ -z "$DISABLE_PYTHON_SCRIPTING" ]; then apk add --no-cache python3; fi

# Copy built minifi distribution from builder
COPY --from=build --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME}
COPY --from=build --chown=${USER}:${USER} ${MINIFI_BASE_DIR}/docker/conf/minifi-log.properties ${MINIFI_HOME}/conf/minifi-log.properties
RUN setup-timezone -z UTC

USER ${USER}
WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ["./bin/minifi.sh", "run"]
