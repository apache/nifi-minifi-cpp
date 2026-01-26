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
from typing import TYPE_CHECKING
from behave.runner import Context
from docker.models.networks import Network

from minifi_test_framework.containers.container import Container
from OpenSSL import crypto

if TYPE_CHECKING:
    from minifi_test_framework.containers.minifi_container import MinifiContainer

DEFAULT_MINIFI_CONTAINER_NAME = "minifi-primary"


class MinifiTestContext(Context):
    containers: dict[str, Container]
    scenario_id: str
    network: Network
    minifi_container_image: str
    resource_dir: str | None
    root_ca_key: crypto.PKey
    root_ca_cert: crypto.X509

    def get_or_create_minifi_container(self, container_name: str) -> MinifiContainer:
        if container_name not in self.containers:
            from minifi_test_framework.containers.minifi_container import MinifiContainer
            self.containers[container_name] = MinifiContainer(container_name, self)
        return self.containers[container_name]

    def get_or_create_default_minifi_container(self) -> MinifiContainer:
        return self.get_or_create_minifi_container(DEFAULT_MINIFI_CONTAINER_NAME)
