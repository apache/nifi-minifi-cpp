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
ENV MINIFI_HOME="FHS"


COPY ${ARCHIVE_LOCATION} /tmp/minifi.rpm
RUN /bin/bash -c "${INSTALL_PACKAGE_CMD}" \
    && rpm -i /tmp/minifi.rpm

USER ${USER}

# Start MiNiFi CPP in the foreground
CMD ["/usr/bin/minifi"]
