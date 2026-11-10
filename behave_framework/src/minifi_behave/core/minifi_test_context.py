#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

from __future__ import annotations

import os

from behave.runner import Context
from cryptography.hazmat.primitives.asymmetric.rsa import RSAPrivateKey
from cryptography.x509 import Certificate
from docker.models.networks import Network
from minifi_behave.containers.container_protocol import ContainerProtocol
from minifi_behave.containers.minifi_protocol import MinifiProtocol

DEFAULT_MINIFI_CONTAINER_NAME = "minifi-primary"


class MinifiContainer(ContainerProtocol, MinifiProtocol):
    pass


class MinifiTestContext(Context):
    containers: dict[str, ContainerProtocol]
    scenario_id: str
    network: Network
    minifi_container_image: str
    resource_dir: str | None
    root_ca_key: RSAPrivateKey
    root_ca_cert: Certificate
    override_default_ca_cert_files: bool

    def get_or_create_minifi_container(self, container_name: str) -> MinifiContainer:
        if container_name not in self.containers:
            if os.name == "nt":
                from minifi_behave.containers.minifi_win_container import MinifiWindowsContainer

                minifi_container = MinifiWindowsContainer(container_name, self)
            else:
                from minifi_behave.containers.deployment import get_deployment_for_image
                from minifi_behave.containers.minifi_linux_container import MinifiLinuxContainer

                minifi_container = MinifiLinuxContainer(
                    container_name, self, get_deployment_for_image(self.minifi_container_image)
                )
            self.containers[container_name] = minifi_container
        return self.containers[container_name]

    def get_or_create_default_minifi_container(self) -> MinifiContainer:
        return self.get_or_create_minifi_container(DEFAULT_MINIFI_CONTAINER_NAME)
