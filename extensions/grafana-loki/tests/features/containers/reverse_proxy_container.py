# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


class ReverseProxyContainer(LinuxContainer):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__(
            "minifi-reverse-proxy:latest",
            f"reverse-proxy-{test_context.scenario_id}",
            test_context.network,
        )
        self.environment = [
            "BASIC_USERNAME=admin",
            "BASIC_PASSWORD=password",
            f"FORWARD_HOST=grafana-loki-server-{test_context.scenario_id}",
            "FORWARD_PORT=3100",
        ]

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        finished_str = "start worker process"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=60,
            bail_condition=lambda: self.exited,
            context=context,
        )
