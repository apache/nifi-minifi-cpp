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

from typing import List, Optional
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class OPCUAServerContainer(Container):
    def __init__(self, test_context: MinifiTestContext, command: Optional[List[str]] = None):
        super().__init__("lordgamez/open62541:1.4.10", f"opcua-server-{test_context.scenario_id}", test_context.network, command=command)

    def deploy(self):
        super().deploy()
        finished_str = "New DiscoveryUrl added: opc.tcp://"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=None)
