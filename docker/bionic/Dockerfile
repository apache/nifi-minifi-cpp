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

# First stage: the build environment
ARG BASE_IMAGE="ubuntu:bionic"

FROM ${BASE_IMAGE} AS build
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ARG MINIFI_VERSION
ARG DOCKER_MAKE_TARGET="all"

# Install the system dependencies needed for a build

ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION
ENV JAVA_HOME /usr/lib/jvm/java-8-openjdk-amd64/

RUN apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y openjdk-8-jdk openjdk-8-source sudo git maven autogen autoconf automake pkgconf libtool libfl-dev libusb-1.0-0-dev \
    libpng-dev libgps-dev libsqliteodbc liblua5.3-dev \
    && mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}


FROM build_deps AS release

# MINIFI_OPTIONS will be passed directly to cmake
# use it to define cmake options (e.g. -DENABLE_AWS=ON -DENABLE_AZURE=ON)
ARG MINIFI_OPTIONS=""
ARG CMAKE_BUILD_TYPE=Release

ENV CC gcc-11
ENV CXX g++-11

# Run bootstrap and build
RUN cd $MINIFI_BASE_DIR \
    && ./bootstrap.sh -t \
    && cd $MINIFI_BASE_DIR/build \
    && cmake -DSTATIC_BUILD= -DSKIP_TESTS=true ${MINIFI_OPTIONS} -DAWS_ENABLE_UNITY_BUILD=OFF -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. \
    && make -j "$(nproc)" ${DOCKER_MAKE_TARGET}
