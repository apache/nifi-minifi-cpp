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
ARG BASE_IMAGE="apacheminificpp:behave"

FROM ${BASE_IMAGE} AS builder
LABEL maintainer="Martin Zink <martinzink@apache.org>"

COPY ./target/release/libminifi_rs_playground.so /opt/minifi/minifi-current/extensions

RUN /opt/minifi/minifi-current/bin/minifi --docs DOCS_OUTPUT

FROM scratch AS docs-export

COPY --from=builder /opt/minifi/minifi-current/DOCS_OUTPUT /
