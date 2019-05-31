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
# Edge required for rocksdb
FROM centos:7 AS builder
MAINTAINER Apache NiFi <dev@nifi.apache.org>

ARG UID
ARG GID
ARG MINIFI_VERSION
ARG MINIFI_SOURCE_CODE
ARG DUMP_LOCATION
ARG ENABLE_JNI

# Install the system dependencies needed for a build

ENV USER root
ENV MINIFI_BASE_DIR /opt/minifi
RUN mkdir -p $MINIFI_BASE_DIR 
USER $USER

RUN yum -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel sudo git which maven


ADD $MINIFI_SOURCE_CODE $MINIFI_BASE_DIR

ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION

# Setup minificpp user
RUN mkdir -p $MINIFI_BASE_DIR 


# Perform the build
RUN cd $MINIFI_BASE_DIR \
	&& ./bootstrap.sh -e -t \
	&& rm -rf build \ 
	&& mkdir build \
	&& cd build \
	&& cmake3 -DUSE_SHARED_LIBS=  -DENABLE_MQTT=ON -DENABLE_LIBRDKAFKA=ON -DPORTABLE=ON -DENABLE_COAP=ON -DCMAKE_BUILD_TYPE=Release -DSKIP_TESTS=true -DENABLE_JNI=$ENABLE_JNI .. \
	&& make -j8 package 

#COPY $MINIFI_BASE_DIR/build/nifi-minifi-cpp-$MINIFI_VERSION-bin.tar.gz $DUMP_LOCATION/nifi-minifi-cpp-centos7-$MINIFI_VERSION-bin.tar.gz