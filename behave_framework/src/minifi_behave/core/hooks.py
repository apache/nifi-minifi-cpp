#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
import os
import types
from pathlib import Path

from behave.model import Scenario
from behave.runner import Context
from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.core.ssl_utils import make_self_signed_cert

import docker

logger = logging.getLogger(__name__)


def get_minifi_container_image():
    if "MINIFI_TAG_PREFIX" in os.environ and "MINIFI_VERSION" in os.environ:
        minifi_tag_prefix = os.environ["MINIFI_TAG_PREFIX"]
        minifi_version = os.environ["MINIFI_VERSION"]
        return "apacheminificpp:" + minifi_tag_prefix + minifi_version
    return "apacheminificpp:behave"


def inject_scenario_id(context: MinifiTestContext, step):
    if "${scenario_id}" in step.name:
        step.name = step.name.replace("${scenario_id}", context.scenario_id)
    if getattr(step, "table", None):
        for row in step.table:
            row.cells = [
                cell.replace("${scenario_id}", context.scenario_id) if "${scenario_id}" in cell else cell
                for cell in row.cells
            ]
    if hasattr(step, "text") and step.text and "${scenario_id}" in step.text:
        step.text = step.text.replace("${scenario_id}", context.scenario_id)


def common_before_scenario(context: Context, scenario: Scenario):
    if "SUPPORTS_WINDOWS" not in scenario.effective_tags and os.name == "nt":
        scenario.skip("No windows support")
        return

    if not hasattr(context, "minifi_container_image"):
        context.minifi_container_image = get_minifi_container_image()

    method_map = {
        "get_or_create_minifi_container": MinifiTestContext.get_or_create_minifi_container,
        "get_or_create_default_minifi_container": MinifiTestContext.get_or_create_default_minifi_container,
    }
    for attr, method in method_map.items():
        if not hasattr(context, attr):
            setattr(context, attr, types.MethodType(method, context))

    logger.info("Running scenario: %s", scenario)
    context.scenario_id = (
        scenario.filename.rsplit("/", 1)[1].split(".")[0] + "-" + str(scenario.parent.scenarios.index(scenario))
    )
    network_name = f"{context.scenario_id}-net"
    docker_client = docker.client.from_env()

    try:
        existing_network = docker_client.networks.get(network_name)
        logger.warning(f"Found existing network '{network_name}'. Removing it first.")
        existing_network.remove()
    except docker.errors.NotFound:
        pass  # No existing network found, which is good.

    context.network = docker_client.networks.create(network_name)
    context.containers = {}
    context.resource_dir = None
    context.root_ca_cert, context.root_ca_key = make_self_signed_cert("root CA")
    context.override_default_ca_cert_files = True

    for step in scenario.steps:
        inject_scenario_id(context, step)


def common_after_scenario(context: MinifiTestContext, scenario: Scenario):
    if hasattr(context, "evidence_path") and os.environ.get("LOGS"):
        header = (
            f"FEATURE  : {scenario.feature.name}\n"
            f"SCENARIO : {scenario.name}\n"
            f"FILE     : {scenario.feature.filename}\n"
            f"LINE     : {scenario.line}\n"
        )

        log_dir_path = Path(os.environ.get("LOGS")) / Path(context.evidence_path)
        scenario_info_path = log_dir_path / "scenario_info.txt"
        with open(scenario_info_path, "w") as f:
            f.write(header)

    if hasattr(context, "containers"):
        for container in context.containers.values():
            container.clean_up()
    if hasattr(context, "network"):
        context.network.remove()


def add_extension_to_minifi_container(extension_name: str, possible_paths: list[str], context: MinifiTestContext):
    new_container_name = f"apacheminificpp:{extension_name}"
    is_windows = os.name == "nt"
    if is_windows:
        lib_filename = f"{extension_name}.dll"
        container_extension_dir = "C:/Program Files/ApacheNiFiMiNiFi/nifi-minifi-cpp/extensions"
    else:
        lib_filename = f"lib{extension_name}.so"
        container_extension_dir = "/opt/minifi/minifi-current/extensions/"

    host_path = None
    for path in possible_paths:
        if os.path.exists(os.path.join(path, lib_filename)):
            host_path = os.path.join(path, lib_filename)
            break

    assert host_path is not None, f"Could not find {lib_filename} in {[p for p in possible_paths]}"

    with open(host_path, "rb") as f:
        lib_content = f.read()

    base_img = get_minifi_container_image()

    if is_windows:
        dockerfile = f"""
FROM {base_img}
COPY ["{lib_filename}", "{container_extension_dir}/{lib_filename}"]
"""
    else:
        dockerfile = f"""
FROM {base_img}
COPY --chown=minificpp:minificpp {lib_filename} {container_extension_dir}
RUN chmod 755 {container_extension_dir}{lib_filename}
"""

    builder = DockerImageBuilder(
        image_tag=new_container_name,
        dockerfile_content=dockerfile,
        files_on_context={lib_filename: lib_content},
    )

    builder.build()
    return new_container_name
