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

ARG MINIFI_VERSION
ARG ARCHIVE_LOCATION
ARG INSTALL_PACKAGE_CMD

ARG UID=1001
ARG GID=1001

ENV USER minificpp
ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME ${MINIFI_BASE_DIR}/minifi-current
ENV MINIFI_VERSIONED_HOME ${MINIFI_BASE_DIR}/nifi-minifi-cpp-${MINIFI_VERSION}


RUN groupadd -g ${GID} ${USER} && useradd -m -g ${GID} ${USER} && \
    install -d -o ${USER} -g ${USER} ${MINIFI_BASE_DIR} && ln -s ${MINIFI_VERSIONED_HOME} ${MINIFI_HOME}

ADD ${ARCHIVE_LOCATION} ${MINIFI_BASE_DIR}
RUN chown -R ${USER}:${USER} /opt/minifi  \
    && /bin/bash -c "${INSTALL_PACKAGE_CMD}"

USER ${USER}
WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ["./bin/minifi.sh", "run"]
