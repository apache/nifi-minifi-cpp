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
FROM centos:7
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

ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION
ENV USER minificpp

# gpsd-devel is in EPEL
RUN yum -y install epel-release && yum -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel python36-devel sudo git which maven make libarchive boost-devel lua-devel libusbx-devel libpng-devel \
    gpsd-devel libpcap-devel ccache ca-certificates

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}

RUN cd $MINIFI_BASE_DIR && \
    ./bootstrap.sh -t && \
    ln -s /usr/bin/ccache /usr/lib64/ccache/c++

# Setup minificpp user
RUN groupadd -g ${GID} ${USER} && useradd -g ${GID} ${USER} && \
    chown -R ${USER}:${USER} ${MINIFI_BASE_DIR} && \
    if [ -d "${MINIFI_BASE_DIR}/.ccache" ]; then mv ${MINIFI_BASE_DIR}/.ccache /home/${USER}/.ccache; fi

USER ${USER}

# Perform the build
RUN cd $MINIFI_BASE_DIR && \
    cd build && \
    source /opt/rh/devtoolset-10/enable && \
    export PATH=/usr/lib64/ccache${PATH:+:${PATH}} && \
    cmake3 -DSTATIC_BUILD= -DSKIP_TESTS=${DOCKER_SKIP_TESTS} ${MINIFI_OPTIONS} -DAWS_ENABLE_UNITY_BUILD=OFF -DEXCLUDE_BOOST=ON -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. && \
    make -j "$(nproc)" package
