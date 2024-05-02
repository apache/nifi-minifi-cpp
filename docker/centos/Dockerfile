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
ARG BASE_IMAGE="centos:7"

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
ARG DOCKER_MAKE_TARGET="all"

# Install the system dependencies needed for a build

ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION
ENV USER minificpp

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}

# gpsd-devel and ccache are in EPEL
RUN ulimit -n 1024000 && yum -y install epel-release && yum -y install sudo git which make libarchive ccache ca-certificates perl-IPC-Cmd && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_GPS=ON"; then yum -y install gpsd-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_JNI=ON"; then yum -y install java-1.8.0-openjdk maven; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_PCAP=ON"; then yum -y install libpcap-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_USB_CAMERA=ON"; then yum -y install libpng-devel libusbx-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_PYTHON_SCRIPTING=ON"; then yum -y install python36-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_SFTP=ON" && [ "${DOCKER_SKIP_TESTS}" == "OFF" ]; then yum -y install java-1.8.0-openjdk maven; fi

RUN cd $MINIFI_BASE_DIR && \
    ./bootstrap.sh -t && \
    ln -s /usr/bin/ccache /usr/lib64/ccache/c++

# Setup minificpp user
RUN groupadd -g ${GID} ${USER} && useradd -g ${GID} ${USER} && \
    chown -R ${USER}:${USER} ${MINIFI_BASE_DIR}

USER ${USER}

# Perform the build
RUN cd $MINIFI_BASE_DIR && \
    cd build && \
    source /opt/rh/devtoolset-11/enable && \
    export PATH=/usr/lib64/ccache${PATH:+:${PATH}} && \
    export CCACHE_DIR=${MINIFI_BASE_DIR}/.ccache && \
    cmake -DSTATIC_BUILD= -DSKIP_TESTS=${DOCKER_SKIP_TESTS} ${MINIFI_OPTIONS} -DAWS_ENABLE_UNITY_BUILD=OFF -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. && \
    make -j "$(nproc)" ${DOCKER_MAKE_TARGET}
