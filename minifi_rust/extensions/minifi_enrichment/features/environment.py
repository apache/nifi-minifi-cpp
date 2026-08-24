# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

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
    add_extension_to_minifi_container("minifi_enrichment", [build_path, deps_build_path], context)


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp:minifi_enrichment"
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
