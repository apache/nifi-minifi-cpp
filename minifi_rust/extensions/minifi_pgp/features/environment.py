# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import os
from pathlib import Path

from minifi_behave.core.hooks import (
    add_extension_to_minifi_container,
    common_after_scenario,
    common_before_scenario,
)


def before_all(context):
    dir_path = os.path.dirname(os.path.realpath(__file__))
    build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/"))
    add_extension_to_minifi_container("minifi_pgp", [build_path], context)


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp:minifi_pgp"
    common_before_scenario(context, scenario)
    context.resource_dir = Path(f"{os.path.dirname(os.path.realpath(__file__))}/..")


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
