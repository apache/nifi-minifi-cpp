# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import logging

from minifi_behave.containers.deployment import NormalDeployment
from minifi_behave.containers.minifi_linux_container import MinifiLinuxContainer
from minifi_behave.core.minifi_test_context import MinifiTestContext

logger = logging.getLogger(__name__)


class MinifiAsPodInKubernetesCluster(MinifiLinuxContainer):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(container_name, test_context, NormalDeployment())
        self.container = test_context.kubernetes_proxy

    def deploy(self, context: MinifiTestContext | None) -> bool:
        logger.debug("Setting up the kind Kubernetes cluster")
        self.container.write_minifi_conf_file("minifi.properties", self._get_properties_file_content())
        self.container.write_minifi_conf_file("minifi-log.properties", self._get_log_properties_file_content())
        self.container.write_minifi_conf_file("config.yml", self.flow_definition.to_yaml())
        self.container.create_helper_objects()
        self.container.load_docker_image("apacheminificpp:docker_test")
        self.container.create_minifi_pod()
        return True
