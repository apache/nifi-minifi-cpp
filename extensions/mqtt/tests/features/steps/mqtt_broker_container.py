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
import re
from textwrap import dedent

from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder
from docker.errors import ContainerError


class MqttBrokerContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        dockerfile = dedent("""\
            FROM {base_image}
            RUN echo 'log_dest stderr' >> /mosquitto-no-auth.conf
            CMD ["/usr/sbin/mosquitto", "--verbose", "--config-file", "/mosquitto-no-auth.conf"]
            """.format(base_image='eclipse-mosquitto:2.0.14'))

        builder = DockerImageBuilder(
            image_tag="minifi-mqtt-broker:latest",
            dockerfile_content=dockerfile
        )
        builder.build()

        super().__init__("minifi-mqtt-broker:latest", f"mqtt-broker-{test_context.scenario_id}", test_context.network)

    def deploy(self):
        super().deploy()
        finished_str = "mosquitto version [0-9\\.]+ running"
        return wait_for_condition(
            condition=lambda: re.search(finished_str, self.get_logs()),
            timeout_seconds=60,
            bail_condition=lambda: self.exited,
            context=None)

    def publish_mqtt_message(self, topic: str, message: str):
        try:
            self.client.containers.run("minifi-mqtt-helper:latest", ["python", "/scripts/publish_mqtt_message.py", self.container_name, topic, message], remove=True, stdout=True, stderr=True, network=self.network.name)
            return True
        except ContainerError as e:
            stdout = e.stdout.decode("utf-8", errors="replace") if hasattr(e, "stdout") and e.stdout else ""
            stderr = e.stderr.decode("utf-8", errors="replace") if hasattr(e, "stderr") and e.stderr else ""
            logging.error(f"Failed to publish mqtt message in mqtt helper docker with error: '{e}', stdout: '{stdout}', stderr: '{stderr}'")
            return False
        except Exception as e:
            logging.error(f"Unexpected error while publishing mqtt message in mqtt helper docker: '{e}'")
            return False
