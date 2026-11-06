# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ARG BASE_IMAGE

FROM ${BASE_IMAGE} AS build
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ENV MINIFI_BASE_DIR /opt/minifi
ENV DEBIAN_FRONTEND=noninteractive
ENV CC=clang
ENV CXX=clang++
ENV CXXFLAGS="-stdlib=libc++"
ENV LDFLAGS="-stdlib=libc++"

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}


RUN apt update && apt install -y sudo python3-venv


RUN cd $MINIFI_BASE_DIR/bootstrap && python3 -m venv venv && . venv/bin/activate \
                                       && pip install -r requirements.txt \
                                       && python main.py --noninteractive --skip-compiler-install --minifi-options="-DENABLE_ALL=ON -DMINIFI_FAIL_ON_WARNINGS=ON"
