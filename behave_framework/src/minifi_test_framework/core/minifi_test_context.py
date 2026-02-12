#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

from __future__ import annotations

import os
from typing import Any
import docker
from behave.runner import Context
from docker.models.networks import Network

from minifi_test_framework.containers.container_protocol import ContainerProtocol
from minifi_test_framework.containers.minifi_protocol import MinifiProtocol


DEFAULT_MINIFI_CONTAINER_NAME = "minifi-primary"


class MinifiContainer(ContainerProtocol, MinifiProtocol):
    pass


class MinifiTestContext(Context):
    containers: dict[str, ContainerProtocol]
    scenario_id: str
    network: Network
    minifi_container_image: str
    resource_dir: str | None
    root_ca_key: Any
    root_ca_cert: Any

    def get_or_create_minifi_container(self, container_name: str) -> MinifiContainer:
        if container_name not in self.containers:
            if os.name == 'nt':
                from minifi_test_framework.containers.minifi_win_container import MinifiWindowsContainer
                minifi_container = MinifiWindowsContainer(container_name, self)
            elif 'MINIFI_INSTALLATION_TYPE=FHS' in str(docker.from_env().images.get(self.minifi_container_image).history()):
                from minifi_test_framework.containers.minifi_fhs_container import MinifiFhsContainer
                minifi_container = MinifiFhsContainer(container_name, self)
            else:
                from minifi_test_framework.containers.minifi_linux_container import MinifiLinuxContainer
                minifi_container = MinifiLinuxContainer(container_name, self)
            self.containers[container_name] = minifi_container
        return self.containers[container_name]

    def get_or_create_default_minifi_container(self) -> MinifiContainer:
        return self.get_or_create_minifi_container(DEFAULT_MINIFI_CONTAINER_NAME)
