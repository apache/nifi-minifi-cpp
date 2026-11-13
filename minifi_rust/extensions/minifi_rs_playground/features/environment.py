# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import os

from minifi_behave.core.hooks import (
    add_extension_to_minifi_container,
    common_after_scenario,
    common_before_scenario,
)


def before_all(context):
    dir_path = os.path.dirname(os.path.realpath(__file__))
    build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/"))
    deps_build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/deps/"))
    add_extension_to_minifi_container("minifi_rs_playground", [build_path, deps_build_path], context)


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp:minifi_rs_playground"
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
