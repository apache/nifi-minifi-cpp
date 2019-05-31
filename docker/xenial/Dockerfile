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
FROM ubuntu:xenial AS builder
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

RUN apt-get update && apt-get install -y openjdk-8-jdk openjdk-8-source sudo git maven


ADD $MINIFI_SOURCE_CODE $MINIFI_BASE_DIR

ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION

# Setup minificpp user
RUN mkdir -p $MINIFI_BASE_DIR 
ENV JAVA_HOME /usr/lib/jvm/java-8-openjdk-amd64/
#ENV PATH $PATH:/usr/lib/jvm/default-jvm/bin


# Perform the build
RUN cd $MINIFI_BASE_DIR \
	&& ./bootstrap.sh -e -t \
	&& rm -rf build \ 
	&& mkdir build \
	&& cd build \
	&& cmake -DUSE_SHARED_LIBS=  -DENABLE_MQTT=ON -DENABLE_LIBRDKAFKA=ON -DPORTABLE=ON -DENABLE_COAP=ON -DCMAKE_BUILD_TYPE=Release -DSKIP_TESTS=true -DENABLE_JNI=$ENABLE_JNI .. \
	&& make -j8 package 