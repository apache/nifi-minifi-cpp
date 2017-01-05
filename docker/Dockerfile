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

FROM ubuntu:16.10
MAINTAINER Apache NiFi <dev@nifi.apache.org>

ARG UID
ARG GID
ARG MINIFI_VERSION
ARG MINIFI_SOURCE_CODE

# Install the system dependencies needed for a build
RUN apt-get update && apt-get install -y build-essential \
	wget \
	gdb \
	libboost-all-dev \
	libxml2-dev \
	vim \
	uuid-dev \
	libleveldb-dev \
	cmake \
	git \
	unzip \
	libgps-dev \
	gpsd \
	gpsd-clients \
	libssl-dev

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi

# Setup minificpp user
RUN addgroup --gid $GID $USER && adduser --uid $UID --gid $GID --disabled-password --gecos "" $USER
RUN mkdir -p $MINIFI_BASE_DIR 

ADD $MINIFI_SOURCE_CODE $MINIFI_BASE_DIR
RUN chown -R $USER:$USER $MINIFI_BASE_DIR

USER $USER

ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION

# Perform the build
RUN cd $MINIFI_BASE_DIR \
	&& mkdir build \
	&& cd build \
	&& cmake .. \
	&& make package \
	&& tar -xzvf $MINIFI_BASE_DIR/build/nifi-minifi-cpp-$MINIFI_VERSION-bin.tar.gz -C $MINIFI_BASE_DIR

# Start MiNiFi CPP in the foreground
CMD cd $MINIFI_HOME && ./bin/minifi.sh run

