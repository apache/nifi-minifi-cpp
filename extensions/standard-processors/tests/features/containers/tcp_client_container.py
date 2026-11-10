# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


class TcpClientContainer(LinuxContainer):
    def __init__(self, test_context: MinifiTestContext):
        cmd = (
            "/bin/sh -c 'apk add netcat-openbsd && "
            "echo TCP client container started; "
            "while true; do echo test_tcp_message | "
            f"nc minifi-primary-{test_context.scenario_id} 10254; "
            "sleep 1; done'"
        )
        super().__init__(
            "alpine:3.17.3",
            f"tcp-client-{test_context.scenario_id}",
            test_context.network,
            cmd,
        )

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        finished_str = "TCP client container started"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=context,
        )
