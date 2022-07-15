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
FROM centos:7 AS build_deps
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

# MINIFI_OPTIONS will be passed directly to cmake
# use it to define cmake options (e.g. -DENABLE_AWS=ON -DENABLE_AZURE=ON)
ARG MINIFI_VERSION

# Install the system dependencies needed for a build

ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION

# gpsd-devel is in EPEL
RUN yum -y install epel-release && yum -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel python36-devel sudo git which maven make libarchive boost-devel lua-devel libusbx-devel libpng-devel \
    gpsd-devel libpcap-devel

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}

FROM build_deps AS release
ARG MINIFI_OPTIONS=""
ARG CMAKE_BUILD_TYPE=Release
# Perform the build
RUN cd $MINIFI_BASE_DIR \
	&& ./bootstrap.sh -t \
	&& cd build \
    && scl enable devtoolset-10 -- cmake3 -DSTATIC_BUILD= -DSKIP_TESTS=true ${MINIFI_OPTIONS} -DAWS_ENABLE_UNITY_BUILD=OFF -DEXCLUDE_BOOST=ON -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" .. \
    && scl enable devtoolset-10 -- make -j "$(nproc)" package

