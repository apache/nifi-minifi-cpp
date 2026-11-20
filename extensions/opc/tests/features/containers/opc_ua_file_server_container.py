# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from pathlib import Path

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.containers.directory import Directory
from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


class OPCUAFileServerContainer(LinuxContainer):
    """OPC UA server exposing the files of its /data directory as FileType nodes in the ns=1;s=Files FileDirectoryType node"""

    IMAGE = "minifi-opcua-file-server:latest"
    DATA_DIR = "/data"

    def __init__(self, test_context: MinifiTestContext):
        builder = DockerImageBuilder(
            image_tag=OPCUAFileServerContainer.IMAGE,
            build_context_path=str(Path(__file__).resolve().parent.parent / "resources" / "opcua-file-server"),
        )
        builder.build()

        super().__init__(
            OPCUAFileServerContainer.IMAGE,
            f"opcua-file-server-{test_context.scenario_id}",
            test_context.network,
        )
        self.data_dir = Directory(OPCUAFileServerContainer.DATA_DIR)
        self.dirs.append(self.data_dir)

    def add_server_file(self, name: str, content: str):
        self.data_dir.add_file(name, content)

    def file_has_content(self, name: str, content: str) -> bool:
        return self.path_with_content_exists(f"{OPCUAFileServerContainer.DATA_DIR}/{name}", content)

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        finished_str = "New DiscoveryUrl added: opc.tcp://"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=context,
        )
