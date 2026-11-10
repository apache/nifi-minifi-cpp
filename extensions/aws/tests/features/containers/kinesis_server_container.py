# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import logging
from pathlib import Path

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.helpers import retry_check, wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext

logger = logging.getLogger(__name__)


class KinesisServerContainer(LinuxContainer):
    def __init__(self, test_context: MinifiTestContext):
        builder = DockerImageBuilder(
            image_tag="minifi-kinesis-mock:latest",
            build_context_path=str(Path(__file__).resolve().parent.parent / "resources" / "kinesis-mock"),
        )
        builder.build()

        super().__init__(
            "minifi-kinesis-mock:latest",
            f"kinesis-server-{test_context.scenario_id}",
            test_context.network,
        )
        self.environment.append("INITIALIZE_STREAMS=test_stream:3")
        self.environment.append("LOG_LEVEL=DEBUG")

    def deploy(self, context: MinifiTestContext | None) -> bool:
        super().deploy(context)
        finished_str = "Starting Kinesis Plain Mock Service on port 4568"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=300,
            bail_condition=lambda: self.exited,
            context=context,
        )

    @retry_check()
    def check_kinesis_server_record_data(self, record_data):
        (code, output) = self.exec_run(["node", "/app/consumer/consumer.js", record_data])
        logger.info(f"Kinesis server returned output: '{output}' with code '{code}'")
        return code == 0
