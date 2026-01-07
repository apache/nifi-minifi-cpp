# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging

from pathlib import Path
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition, retry_check
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder


class KinesisServerContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        builder = DockerImageBuilder(
            image_tag="minifi-kinesis-mock:latest",
            build_context_path=str(Path(__file__).resolve().parent.parent / "resources" / "kinesis-mock")
        )
        builder.build()

        super().__init__("minifi-kinesis-mock:latest", f"kinesis-server-{test_context.scenario_id}", test_context.network)
        self.environment.append("INITIALIZE_STREAMS=test_stream:3")
        self.environment.append("LOG_LEVEL=DEBUG")

    def deploy(self):
        super().deploy()
        finished_str = "Starting Kinesis Plain Mock Service on port 4568"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=300,
            bail_condition=lambda: self.exited,
            context=None)

    @retry_check()
    def check_kinesis_server_record_data(self, record_data):
        (code, output) = self.exec_run(["node", "/app/consumer/consumer.js", record_data])
        logging.info(f"Kinesis server returned output: '{output}' with code '{code}'")
        return code == 0
