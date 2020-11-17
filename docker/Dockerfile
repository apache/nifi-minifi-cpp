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
FROM alpine:3.12 AS build_deps
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
  && cmake -DDISABLE_LIBARCHIVE=ON -DDISABLE_SCRIPTING=ON -DENABLE_LIBRDKAFKA=ON -DSKIP_TESTS=true -DCMAKE_BUILD_TYPE=MinSizeRel .. \
  && make -j$(nproc) package \
  && tar -xzvf ${MINIFI_BASE_DIR}/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz -C ${MINIFI_BASE_DIR}


# Build stage of normal image
FROM build_deps AS build_release
ARG ENABLE_JNI
RUN cd ${MINIFI_BASE_DIR} \
  && mkdir build \
  && cd build \
  && cmake -DDISABLE_JEMALLOC=ON -DENABLE_AWS=ON -DSTATIC_BUILD= -DSKIP_TESTS=true -DENABLE_JNI=${ENABLE_JNI} -DENABLE_LIBRDKAFKA=ON .. \
  && make -j$(nproc) package \
  && tar -xzvf ${MINIFI_BASE_DIR}/build/nifi-minifi-cpp-${MINIFI_VERSION}-bin.tar.gz -C ${MINIFI_BASE_DIR}


# Common runtime image dependencies
# Edge required for rocksdb
FROM alpine:3.12 AS common_runtime_deps

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
