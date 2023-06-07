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
import datetime
import sys
import shortuuid
import os
import platform

sys.path.append('../minifi')

from MiNiFi_integration_test_driver import MiNiFi_integration_test  # noqa: E402
from minifi import *  # noqa
from cluster.ImageStore import ImageStore  # noqa
from cluster.DockerTestDirectoryBindings import DockerTestDirectoryBindings  # noqa
from cluster.KubernetesProxy import KubernetesProxy  # noqa


def inject_feature_id(context, step):
    if "${feature_id}" in step.name:
        step.name = step.name.replace("${feature_id}", context.feature_id)
    if step.table:
        for row in step.table:
            for i in range(len(row.cells)):
                if "${feature_id}" in row.cells[i]:
                    row.cells[i] = row.cells[i].replace("${feature_id}", context.feature_id)


def before_scenario(context, scenario):
    if "skip" in scenario.effective_tags:
        scenario.skip("Marked with @skip")
        return

    logging.info("Integration test setup at {time:%H:%M:%S.%f}".format(time=datetime.datetime.now()))
    context.test = MiNiFi_integration_test(context=context, feature_id=context.feature_id)
    for step in scenario.steps:
        inject_feature_id(context, step)


def after_scenario(context, scenario):
    if "skip" in scenario.effective_tags:
        logging.info("Scenario was skipped, no need for clean up.")
        return

    logging.info("Integration test teardown at {time:%H:%M:%S.%f}".format(time=datetime.datetime.now()))
    context.test.cleanup()
    context.directory_bindings.cleanup_io()
    if context.kubernetes_proxy:
        context.kubernetes_proxy.delete_pods()


def before_all(context):
    context.config.setup_logging()
    context.image_store = ImageStore()
    context.kubernetes_proxy = None


def before_feature(context, feature):
    if "x86_x64_only" in feature.tags:
        is_x86 = platform.machine() in ("i386", "AMD64", "x86_64")
        if not is_x86:
            feature.skip("This feature is only x86/x64 compatible")
    feature_id = shortuuid.uuid()
    context.feature_id = feature_id
    context.directory_bindings = DockerTestDirectoryBindings(feature_id)
    context.directory_bindings.create_new_data_directories()
    if "requires.kubernetes.cluster" in feature.tags:
        context.kubernetes_proxy = KubernetesProxy(
            context.directory_bindings.get_data_directories(context.feature_id)["kubernetes_temp_dir"],
            os.path.join(os.environ['TEST_DIRECTORY'], 'resources', 'kubernetes', 'pods-etc'))
        context.kubernetes_proxy.create_config(context.directory_bindings.get_directory_bindings(context.feature_id))
        context.kubernetes_proxy.start_cluster()


def after_feature(context, feature):
    if "requires.kubernetes.cluster" in feature.tags and context.kubernetes_proxy:
        context.kubernetes_proxy.cleanup()
        context.kubernetes_proxy = None
