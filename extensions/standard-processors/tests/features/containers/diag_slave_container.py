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

import logging
from textwrap import dedent

from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class DiagSlave(Container):
    def __init__(self, test_context: MinifiTestContext):
        dockerfile = dedent("""\
FROM panterdsd/diagslave:latest
RUN pip install modbus-cli
ENV PROTOCOL=tcp
""")

        builder = DockerImageBuilder(
            image_tag="minifi-diag-slave-tcp:latest",
            dockerfile_content=dockerfile
        )
        builder.build()

        super().__init__("minifi-diag-slave-tcp:latest", f"diag-slave-tcp-{test_context.scenario_id}", test_context.network)

    def deploy(self):
        super().deploy()
        finished_str = "Server started up successfully."
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=None
        )

    def set_value_on_plc_with_modbus(self, modbus_cmd):
        (code, output) = self.exec_run(["modbus", "localhost", modbus_cmd])
        logging.info("Modbus command '%s' output: %s", modbus_cmd, output)
        return code == 0
