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
ARG BASE_ALPINE_IMAGE="alpine:3.13"
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
  && cmake -DENABLE_PYTHON=OFF -DDISABLE_SCRIPTING=ON -DENABLE_LIBRDKAFKA=ON -DENABLE_AWS=ON -DAWS_ENABLE_UNITY_BUILD=OFF -DSKIP_TESTS=true -DEXCLUDE_BOOST=ON -DCMAKE_BUILD_TYPE=MinSizeRel .. \
  && make -j$(nproc) package \
  && tar -xzvf ${MINIFI_BASE_DIR}/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz -C ${MINIFI_BASE_DIR}


# Build stage of normal image
FROM build_deps AS build_release
ARG ENABLE_ALL=OFF
ARG ENABLE_PYTHON=OFF
ARG ENABLE_OPS=ON
ARG ENABLE_JNI=OFF
ARG ENABLE_OPENCV=OFF
ARG ENABLE_OPC=OFF
ARG ENABLE_GPS=OFF
ARG ENABLE_COAP=OFF
ARG ENABLE_WEL=OFF
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
ARG DISABLE_CURL=OFF
ARG DISABLE_JEMALLOC=ON
ARG DISABLE_CIVET=OFF
ARG DISABLE_EXPRESSION_LANGUAGE=OFF
ARG DISABLE_ROCKSDB=OFF
ARG DISABLE_LIBARCHIVE=OFF
ARG DISABLE_LZMA=OFF
ARG DISABLE_BZIP2=OFF
ARG DISABLE_SCRIPTING=OFF
ARG DISABLE_PYTHON_SCRIPTING=OFF
ARG DISABLE_CONTROLLER=OFF
RUN cd ${MINIFI_BASE_DIR} \
  && mkdir build \
  && cd build \
  && cmake -DSTATIC_BUILD= -DSKIP_TESTS=true -DENABLE_ALL=${ENABLE_ALL} -DENABLE_PYTHON=${ENABLE_PYTHON} -DENABLE_OPS=${ENABLE_OPS} \
    -DENABLE_JNI=${ENABLE_JNI} -DENABLE_OPENCV=${ENABLE_OPENCV} -DENABLE_OPC=${ENABLE_OPC} -DENABLE_GPS=${ENABLE_GPS} -DENABLE_COAP=${ENABLE_COAP} \
    -DENABLE_WEL=${ENABLE_WEL} -DENABLE_SQL=${ENABLE_SQL} -DENABLE_MQTT=${ENABLE_MQTT} -DENABLE_PCAP=${ENABLE_PCAP} \
    -DENABLE_LIBRDKAFKA=${ENABLE_LIBRDKAFKA} -DENABLE_SENSORS=${ENABLE_SENSORS} \
    -DENABLE_USB_CAMERA=${ENABLE_USB_CAMERA} -DENABLE_TENSORFLOW=${ENABLE_TENSORFLOW} -DENABLE_AWS=${ENABLE_AWS} \
    -DENABLE_BUSTACHE=${ENABLE_BUSTACHE} -DENABLE_SFTP=${ENABLE_SFTP} -DENABLE_OPENWSMAN=${ENABLE_OPENWSMAN} -DENABLE_AZURE=${ENABLE_AZURE} \
    -DDISABLE_CURL=${DISABLE_CURL} -DDISABLE_JEMALLOC=${DISABLE_JEMALLOC} -DDISABLE_CIVET=${DISABLE_CIVET} \
    -DDISABLE_EXPRESSION_LANGUAGE=${DISABLE_EXPRESSION_LANGUAGE} -DDISABLE_ROCKSDB=${DISABLE_ROCKSDB} \
    -DDISABLE_LIBARCHIVE=${DISABLE_LIBARCHIVE} -DDISABLE_LZMA=${DISABLE_LZMA} -DDISABLE_BZIP2=${DISABLE_BZIP2} \
    -DDISABLE_SCRIPTING=${DISABLE_SCRIPTING} -DDISABLE_PYTHON_SCRIPTING=${DDISABLE_PYTHON_SCRIPTING} -DDISABLE_CONTROLLER=${DISABLE_CONTROLLER} \
    -DAWS_ENABLE_UNITY_BUILD=OFF -DEXCLUDE_BOOST=ON -DCMAKE_BUILD_TYPE=Release .. \
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
ARG ENABLE_GPS=OFF
ARG ENABLE_JNI=OFF
ARG ENABLE_PCAP=OFF
ARG ENABLE_USB_CAMERA=OFF
ARG ENABLE_OPENCV=OFF
ARG ENABLE_PYTHON=OFF
ARG ENABLE_BUSTACHE=OFF
ARG DISABLE_SCRIPTING=OFF
RUN apk --update --no-cache upgrade && apk add --update --no-cache libstdc++ && \
    if [ "$ENABLE_GPS" = "ON" ]; then apk add --update --no-cache gpsd; fi && \
    if [ "$ENABLE_JNI" = "ON" ]; then apk add --update --no-cache openjdk8-jre-base; fi && \
    if [ "$ENABLE_PCAP" = "ON" ]; then apk add --update --no-cache libpcap; fi && \
    if [ "$ENABLE_USB_CAMERA" = "ON" ]; then apk add --update --no-cache libpng libusb; fi && \
    if [ "$ENABLE_OPENCV" = "ON" ] || [ "$ENABLE_BUSTACHE" = "ON" ]; then apk add --update --no-cache boost; fi && \
    if [ "$ENABLE_PYTHON" = "ON" ] || [ "$DISABLE_SCRIPTING" = "OFF" ]; then apk add --update --no-cache python3-dev; fi

# Copy built minifi distribution from builder
COPY --from=build_release --chown=${USER}:${USER} ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME}

USER ${USER}
WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ./bin/minifi.sh run
