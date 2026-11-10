# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from containers.couchbase_server_container import CouchbaseServerContainer
from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.hooks import common_after_scenario, common_before_scenario

import docker


def before_all(context):
    dockerfile = """
    FROM python:3.13-slim-bookworm
    RUN pip install couchbase==4.3.5 requests"""
    builder = DockerImageBuilder(image_tag="minifi-couchbase-helper:latest", dockerfile_content=dockerfile)
    builder.build()
    docker.from_env().images.pull(CouchbaseServerContainer.IMAGE)


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
