# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import platform

from containers.splunk_container import SplunkContainer
from minifi_behave.core.hooks import common_after_scenario, common_before_scenario

import docker


def before_feature(context, feature):
    if "x86_x64_only" in feature.tags:
        is_x86 = platform.machine() in ("i386", "AMD64", "x86_64")
        if not is_x86:
            feature.skip("This feature is only x86/x64 compatible")
            return

    docker.from_env().images.pull(SplunkContainer.IMAGE)


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
