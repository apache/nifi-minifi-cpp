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

ARG MINIFI_VERSION

# Install the system dependencies needed for a build

ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION

RUN yum -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel python36-devel sudo git which maven make libarchive

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}

FROM build_deps AS release
ARG ENABLE_JNI
# Perform the build
RUN cd $MINIFI_BASE_DIR \
	&& ./bootstrap.sh -t \
	&& cd build \
	&& scl enable devtoolset-10 -- cmake3 -DUSE_SHARED_LIBS=  -DENABLE_MQTT=ON -DENABLE_LIBRDKAFKA=ON -DPORTABLE=ON -DENABLE_COAP=ON -DCMAKE_BUILD_TYPE=Release -DSKIP_TESTS=true -DENABLE_JNI=$ENABLE_JNI .. \
	&& scl enable devtoolset-10 -- make -j$(nproc) package

