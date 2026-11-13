#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

import logging
from textwrap import dedent

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext

logger = logging.getLogger(__name__)


class DiagSlave(LinuxContainer):
    def __init__(self, test_context: MinifiTestContext):
        dockerfile = dedent("""\
FROM panterdsd/diagslave:latest
RUN pip install modbus-cli
ENV PROTOCOL=tcp
""")

        builder = DockerImageBuilder(image_tag="minifi-diag-slave-tcp:latest", dockerfile_content=dockerfile)
        builder.build()

        super().__init__(
            "minifi-diag-slave-tcp:latest",
            f"diag-slave-tcp-{test_context.scenario_id}",
            test_context.network,
        )

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        finished_str = "Server started up successfully."
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=context,
        )

    def set_value_on_plc_with_modbus(self, modbus_cmd):
        (code, output) = self.exec_run(["modbus", "localhost", modbus_cmd])
        logger.info("Modbus command '%s' output: %s", modbus_cmd, output)
        return code == 0
