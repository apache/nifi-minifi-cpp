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
ARG BASE_IMAGE
FROM ${BASE_IMAGE} as base_image
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"


ENV USER minificpp

USER ${USER}
WORKDIR ${MINIFI_HOME}

RUN python3 -m venv venv
RUN . ./venv/bin/activate && pip install --upgrade pip && pip install numpy "langchain<=0.17.0"

# Start MiNiFi CPP in the foreground
CMD ["/bin/bash", "-c", "source ./venv/bin/activate && ./bin/minifi.sh run"]
