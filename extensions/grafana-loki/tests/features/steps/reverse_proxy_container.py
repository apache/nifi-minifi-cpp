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

from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition


class ReverseProxyContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("minifi-reverse-proxy:latest", f"reverse-proxy-{test_context.scenario_id}", test_context.network)
        self.environment = [
            "BASIC_USERNAME=admin",
            "BASIC_PASSWORD=password",
            f"FORWARD_HOST=grafana-loki-server-{test_context.scenario_id}",
            "FORWARD_PORT=3100",
        ]

    def deploy(self):
        super().deploy()
        finished_str = "start worker process"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=60,
            bail_condition=lambda: self.exited,
            context=None)
