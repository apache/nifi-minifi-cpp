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

FROM alpine:3.5
MAINTAINER Apache NiFi <dev@nifi.apache.org>

ARG UID
ARG GID
ARG MINIFI_VERSION
ARG MINIFI_PACKAGE

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME $MINIFI_BASE_DIR/nifi-minifi-cpp-$MINIFI_VERSION

# Setup minificpp user
RUN adduser -u $UID -g $GID -D $USER
RUN mkdir -p $MINIFI_HOME 

ADD $MINIFI_PACKAGE $MINIFI_BASE_DIR
RUN chown -R $USER:$USER $MINIFI_HOME

USER $USER

# Startup NiFi-MiNiFi-CPP
CMD $MINIFI_HOME/bin/minifi.sh run
