# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from containers.s3_server_container import S3ServerContainer
from minifi_behave.core.hooks import common_after_scenario, common_before_scenario

import docker

# These hooks are executed by behave before and after each scenario
# The common_before_scenario and common_after_scenario must be called for proper setup and tear down


def before_feature(context, feature):
    if feature.filename.endswith("s3.feature"):
        docker.from_env().images.pull(S3ServerContainer.IMAGE)


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
