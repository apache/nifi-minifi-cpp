ARG BASE_IMAGE

FROM ${BASE_IMAGE} AS build
LABEL maintainer="Apache NiFi <dev@nifi.apache.org>"

ENV MINIFI_BASE_DIR=/opt/minifi
ENV DEBIAN_FRONTEND=noninteractive

RUN mkdir -p $MINIFI_BASE_DIR
COPY . ${MINIFI_BASE_DIR}


RUN apt update && apt install -y sudo python3-venv


RUN cd $MINIFI_BASE_DIR/bootstrap && python3 -m venv venv && . venv/bin/activate \
                                       && pip install -r requirements.txt \
                                       && python main.py --noninteractive --skip-compiler-install --minifi-options="-DENABLE_ALL=ON -DMINIFI_FAIL_ON_WARNINGS=ON"
