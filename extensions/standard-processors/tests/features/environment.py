# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
import os
import platform

from minifi_behave.core.hooks import common_after_scenario, common_before_scenario

import docker

# These hooks are executed by behave before and after each scenario
# The common_before_scenario and common_after_scenario must be called for proper setup and tear down


def before_feature(context, feature):
    if "x86_x64_only" in feature.tags:
        is_x86 = platform.machine() in ("i386", "AMD64", "x86_64")
        if not is_x86:
            feature.skip("This feature is only x86/x64 compatible")


def is_minifi_image_alpine_based(context):
    if os.name == "nt":
        return False
    client: docker.DockerClient = docker.from_env()
    container = client.containers.create(
        image=context.minifi_container_image,
        command=["cat", "/etc/os-release"],
    )
    try:
        container.start()
        result = container.logs()
        container.remove(force=True)
    except docker.errors.APIError:
        container.remove(force=True)
        return False

    return "alpine" in result.decode("utf-8").lower()


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)
    if "ALPINE_ONLY" in scenario.tags and not is_minifi_image_alpine_based(context):
        scenario.skip("This scenario is only compatible with Alpine-based Minifi images")


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
