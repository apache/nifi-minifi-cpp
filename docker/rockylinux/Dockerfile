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
ARG DOCKER_CREATE_RPM=ON
ARG DOCKER_USE_CONAN=OFF
ARG DOCKER_NIFI_CONAN_USER=

ENV MINIFI_BASE_DIR=/opt/minifi
ENV MINIFI_HOME=$MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION
ENV USER=minificpp

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}

# Install the system dependencies needed for a build
# ccache is in EPEL
RUN dnf -y install epel-release && dnf -y install gcc-toolset-14 gcc-toolset-14-libatomic-devel sudo git which make libarchive ccache ca-certificates perl patch bison flex libtool cmake rpmdevtools && \
    if echo "$MINIFI_OPTIONS" | grep -q "MINIFI_RUST=ON"; then dnf -y install rust cargo clang; fi && \
    if [ "${DOCKER_USE_CONAN}" == "ON" ]; then dnf -y install python3.12; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_ALL=ON\|ENABLE_PYTHON_SCRIPTING=ON\|ENABLE_OPC=ON"; then dnf -y --enablerepo=devel install python3.12-devel; fi && \
    if echo "$MINIFI_OPTIONS" | grep -q "ENABLE_ALL=ON\|ENABLE_SFTP=ON" && [ "${DOCKER_SKIP_TESTS}" == "OFF" ]; then dnf -y install java-1.8.0-openjdk maven; fi

RUN cd $MINIFI_BASE_DIR && \
    ln -sfn /usr/bin/ccache /usr/lib64/ccache/c++

# Setup minificpp user
RUN groupadd -g ${GID} ${USER} && useradd -g ${GID} ${USER} && \
    chown -R ${USER}:${USER} ${MINIFI_BASE_DIR}

USER ${USER}

RUN --mount=type=secret,id=nifi_conan_password,mode=0444 echo "DOCKER_USE_CONAN=${DOCKER_USE_CONAN}" && if [ "${DOCKER_USE_CONAN}" == "ON" ]; then \
        set -e; \
        cd ${MINIFI_BASE_DIR}/bootstrap && \
        python3.12 -m venv venv && \
        source /opt/rh/gcc-toolset-14/enable && \
        source venv/bin/activate && \
        pip3.12 install -r requirements.txt && \
        python3.12 main.py --skip-compiler-install --run-conan-install --minifi-options="${MINIFI_OPTIONS}" && \
        if [ -n "${DOCKER_NIFI_CONAN_USER}" ] && [ -f /run/secrets/nifi_conan_password ]; then \
            conan remote login nifi-conan ${DOCKER_NIFI_CONAN_USER} -p "$(cat /run/secrets/nifi_conan_password)" || true; \
            conan upload "*" -r nifi-conan --confirm || true; \
        fi; \
    fi

# Perform the build
RUN cd $MINIFI_BASE_DIR && \
    mkdir -p build && \
    cd build && \
    source /opt/rh/gcc-toolset-14/enable && \
    export PATH=/usr/lib64/ccache${PATH:+:${PATH}} && \
    export CCACHE_DIR=${MINIFI_BASE_DIR}/.ccache && \
    if [ "${DOCKER_USE_CONAN}" == "ON" ]; then CONAN_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake"; fi && \
    cmake -DSTATIC_BUILD= -DSKIP_TESTS=${DOCKER_SKIP_TESTS} ${CONAN_TOOLCHAIN:-} ${MINIFI_OPTIONS} -DPython_EXECUTABLE=/usr/bin/python3.12 -DAWS_ENABLE_UNITY_BUILD=OFF -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. && \
    make -j "$(nproc)" ${DOCKER_MAKE_TARGET} && \
    if [[ "${DOCKER_CREATE_RPM}" == "ON" ]] ; then \
        cmake -DMINIFI_PACKAGING_TYPE=RPM .. && \
        make -j "$(nproc)" package && \
        ../packaging/rpm/check_rpm_contents.sh *.rpm expected-rpm-contents.txt; \
    fi
