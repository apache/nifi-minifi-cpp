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
ENV CONDA_HOME /opt/conda
ENV LD_LIBRARY_PATH /opt/conda/lib
ENV MINIFI_BASE_DIR /opt/minifi
ENV MINIFI_HOME ${MINIFI_BASE_DIR}/minifi-current
ENV PATH ${CONDA_HOME}/bin:${PATH}

USER root

RUN wget https://repo.anaconda.com/archive/Anaconda3-2023.09-0-Linux-x86_64.sh -P /tmp \
    && echo "6c8a4abb36fbb711dc055b7049a23bbfd61d356de9468b41c5140f8a11abd851 /tmp/Anaconda3-2023.09-0-Linux-x86_64.sh" | sha256sum -c \
    && bash /tmp/Anaconda3-2023.09-0-Linux-x86_64.sh -b -p /opt/conda  \
    && chown -R ${USER}:${USER} /opt/conda

USER ${USER}

RUN ${CONDA_HOME}/bin/conda init bash
RUN ${CONDA_HOME}/bin/conda install "langchain<=0.17.0" -c conda-forge

WORKDIR ${MINIFI_HOME}

# Start MiNiFi CPP in the foreground
CMD ["/bin/bash", "-c", "/opt/minifi/minifi-current/bin/minifi.sh run"]
