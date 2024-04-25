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
ARG BASE_IMAGE="rockylinux:8"

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

ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION
ENV USER minificpp

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}

# Install the system dependencies needed for a build
# gpsd-devel and ccache are in EPEL
RUN dnf -y install epel-release && dnf -y install gcc-toolset-12 sudo git which make libarchive ccache ca-certificates perl patch bison flex libtool cmake && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_ALL=ON"; then dnf -y --enablerepo=devel install gpsd-devel libpng-devel libusbx-devel python3-devel java-1.8.0-openjdk maven libpcap-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_GPS=ON"; then dnf -y install gpsd-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_JNI=ON"; then dnf -y install java-1.8.0-openjdk maven; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_PCAP=ON"; then dnf -y --enablerepo=devel install libpcap-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_USB_CAMERA=ON"; then dnf -y install libpng-devel libusbx-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_PYTHON_SCRIPTING=ON"; then dnf -y install python3-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_SFTP=ON" && [ "${DOCKER_SKIP_TESTS}" == "OFF" ]; then dnf -y install java-1.8.0-openjdk maven; fi

RUN cd $MINIFI_BASE_DIR && \
    ln -s /usr/bin/ccache /usr/lib64/ccache/c++

# Setup minificpp user
RUN groupadd -g ${GID} ${USER} && useradd -g ${GID} ${USER} && \
    chown -R ${USER}:${USER} ${MINIFI_BASE_DIR}

RUN ls -lah ${MINIFI_BASE_DIR}/docker
RUN patch -p1 /opt/rh/gcc-toolset-12/root/usr/include/c++/12/bits/basic_string.tcc ${MINIFI_BASE_DIR}/thirdparty/libstdc++/avoid_bogus_Wrestrict_PR105651.patch

USER ${USER}

# Perform the build
RUN cd $MINIFI_BASE_DIR && \
    mkdir build && \
    cd build && \
    source /opt/rh/gcc-toolset-12/enable && \
    export PATH=/usr/lib64/ccache${PATH:+:${PATH}} && \
    export CCACHE_DIR=${MINIFI_BASE_DIR}/.ccache && \
    cmake -DSTATIC_BUILD= -DSKIP_TESTS=${DOCKER_SKIP_TESTS} ${MINIFI_OPTIONS} -DAWS_ENABLE_UNITY_BUILD=OFF -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. && \
    make -j "$(nproc)" ${DOCKER_MAKE_TARGET}
