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

from minifi_test_framework.containers.minifi_container import MinifiContainer
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class MinifiAsPodInKubernetesCluster(MinifiContainer):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(container_name, test_context)
        self.container = test_context.kubernetes_proxy

    def deploy(self) -> bool:
        logging.debug('Setting up the kind Kubernetes cluster')
        self.container.write_minifi_conf_file("minifi.properties", self._get_properties_file_content())
        self.container.write_minifi_conf_file("minifi-log.properties", self._get_log_properties_file_content())
        self.container.write_minifi_conf_file("config.yml", self.flow_definition.to_yaml())
        self.container.create_helper_objects()
        self.container.load_docker_image("apacheminificpp:docker_test")
        self.container.create_minifi_pod()
        return True
