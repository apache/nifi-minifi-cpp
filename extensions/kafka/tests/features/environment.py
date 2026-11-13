# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from containers.kafka_server_container import KafkaServer
from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.hooks import common_after_scenario, common_before_scenario

import docker


def before_all(context):
    dockerfile = """
    FROM python:3.13-slim-bookworm
    RUN pip install confluent-kafka"""
    builder = DockerImageBuilder(image_tag="minifi-kafka-helper:latest", dockerfile_content=dockerfile)
    builder.build()
    docker.from_env().images.pull(KafkaServer.IMAGE)


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)
    context.override_default_ca_cert_files = False


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
