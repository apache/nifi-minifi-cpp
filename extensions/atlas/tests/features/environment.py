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
import platform

import docker

from containers.atlas_server_container import AtlasServerContainer
from minifi_behave.core.hooks import common_before_scenario
from minifi_behave.core.hooks import common_after_scenario

# Atlas is heavy (~90s cold start), so instead of one container per scenario we boot ONE
# shared Atlas in before_all and reuse it for the whole (single, merged) atlas feature file,
# clearing its entities between scenarios. before_scenario/after_scenario still delegate the
# generic per-scenario setup/teardown to common_before_scenario/common_after_scenario; we only
# add wiring to keep the shared Atlas reachable and clean.

ATLAS_NETWORK_NAME = "atlas-shared-net"


def before_all(context):
    # Boot the single shared Atlas once. Pull first so image-pull latency doesn't eat into the
    # cold-start budget, then create it on its own persistent network and wait until ACTIVE.
    docker_client = docker.from_env()
    docker_client.images.pull(AtlasServerContainer.IMAGE)

    try:
        docker_client.networks.get(ATLAS_NETWORK_NAME).remove()
        logging.warning(f"Removed stale network '{ATLAS_NETWORK_NAME}'.")
    except docker.errors.NotFound:
        pass

    context.atlas_network = docker_client.networks.create(ATLAS_NETWORK_NAME)
    context.atlas = AtlasServerContainer(context.atlas_network)
    assert context.atlas.deploy(None) or context.atlas.log_app_output(), \
        "Shared Atlas server did not become available - see extensions/atlas/tests/features/README.md."


def after_all(context):
    if getattr(context, "atlas", None) is not None:
        context.atlas.clean_up()
    if getattr(context, "atlas_network", None) is not None:
        try:
            context.atlas_network.remove()
        except docker.errors.APIError as exc:
            logging.warning(f"Could not remove '{ATLAS_NETWORK_NAME}': {exc}")


def before_feature(context, feature):
    if "x86_x64_only" in feature.tags:
        is_x86 = platform.machine() in ("i386", "AMD64", "x86_64")
        if not is_x86:
            feature.skip("This feature is only x86/x64 compatible")


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)
    if getattr(scenario, "should_skip", False):
        return
    # Run this scenario's MiNiFi/NiFi/Kafka containers on the SHARED Atlas network so they can
    # resolve `atlas-shared` by name. We deliberately do NOT connect the running Atlas to the
    # per-scenario network instead: multi-homing Atlas breaks host-side access to its published
    # REST port (the daemon starts dropping/refusing the port-forwarded connections). Keeping
    # Atlas single-homed and pointing the scenario's containers at its network sidesteps that.
    # The per-scenario network common_before_scenario just created goes unused; stash it so
    # after_scenario can let the framework remove it (instead of the shared network).
    context.scenario_network = context.network
    context.network = context.atlas_network
    # Wipe entities left by the previous scenario for a clean graph.
    context.atlas.clear_entities()


def after_scenario(context, scenario):
    # Point context.network back at the disposable per-scenario network so common_after_scenario
    # removes THAT, not the shared Atlas network (which must survive for the remaining scenarios).
    # Scenario containers are on the shared network; common_after_scenario still removes them via
    # context.containers, which disconnects them from it.
    if getattr(context, "scenario_network", None) is not None:
        context.network = context.scenario_network
        context.scenario_network = None
    common_after_scenario(context, scenario)
