# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import os

from containers.opc_ua_server_container import OPCUAServerContainer
from minifi_behave.core.hooks import common_after_scenario, common_before_scenario

import docker


def before_all(context):
    docker.from_env().images.pull(OPCUAServerContainer.IMAGE)


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)
    context.resource_dir = os.path.join(os.path.dirname(__file__), "resources")


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
