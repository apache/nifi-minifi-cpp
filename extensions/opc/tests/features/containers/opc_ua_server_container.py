# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


class OPCUAServerContainer(LinuxContainer):
    IMAGE = "lordgamez/open62541:1.5.2"

    def __init__(self, test_context: MinifiTestContext, command: list[str] | None = None):
        super().__init__(
            OPCUAServerContainer.IMAGE,
            f"opcua-server-{test_context.scenario_id}",
            test_context.network,
            command=command,
        )

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        finished_str = "New DiscoveryUrl added: opc.tcp://"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=context,
        )
