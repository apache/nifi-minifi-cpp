# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import os

from minifi_behave.core.hooks import (
    common_after_scenario,
    common_before_scenario,
    get_minifi_container_image,
)
from steps.kubernetes_proxy import KubernetesProxy

import docker


def before_feature(context, feature):
    if "rpm" in os.environ["MINIFI_TAG_PREFIX"]:
        feature.skip("This feature is not yet supported on RPM installed images")

    minifi_image = docker.from_env().images.get(get_minifi_container_image())
    minifi_image.tag("apacheminificpp", "docker_test")

    context.kubernetes_proxy = KubernetesProxy()
    context.kubernetes_proxy.delete_cluster()


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)
    context.kubernetes_proxy.create_cluster()


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
    context.kubernetes_proxy.delete_cluster()
