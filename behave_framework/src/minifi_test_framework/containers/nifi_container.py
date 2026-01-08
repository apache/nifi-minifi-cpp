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

import io
import gzip
from typing import List, Optional
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.minifi.nifi_flow_definition import NifiFlowDefinition


class NifiContainer(Container):
    NIFI_VERSION = '2.7.2'

    def __init__(self, test_context: MinifiTestContext, command: Optional[List[str]] = None, use_ssl: bool = False):
        self.flow_definition = NifiFlowDefinition()
        name = f"nifi-{test_context.scenario_id}"
        if use_ssl:
            entry_command = (r"sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' "
                             r"-e 's/^\(nifi.remote.input.secure\)=.*/\1=true/' "
                             r"-e 's/^\(nifi.sensitive.props.key\)=.*/\1=secret_key_12345/' "
                             r"-e 's/^\(nifi.web.https.port\)=.*/\1=8443/' "
                             r"-e 's/^\(nifi.web.https.host\)=.*/\1={name}/' "
                             r"-e 's/^\(nifi.security.keystore\)=.*/\1=\/tmp\/resources\/keystore.jks/' "
                             r"-e 's/^\(nifi.security.keystoreType\)=.*/\1=jks/' "
                             r"-e 's/^\(nifi.security.keystorePasswd\)=.*/\1=passw0rd1!/' "
                             r"-e 's/^\(nifi.security.keyPasswd\)=.*/#\1=passw0rd1!/' "
                             r"-e 's/^\(nifi.security.truststore\)=.*/\1=\/tmp\/resources\/truststore.jks/' "
                             r"-e 's/^\(nifi.security.truststoreType\)=.*/\1=jks/' "
                             r"-e 's/^\(nifi.security.truststorePasswd\)=.*/\1=passw0rd1!/' "
                             r"-e 's/^\(nifi.remote.input.socket.port\)=.*/\1=10443/' /opt/nifi/nifi-current/conf/nifi.properties && "
                             r"cp /tmp/nifi_config/flow.json.gz /opt/nifi/nifi-current/conf && /opt/nifi/nifi-current/bin/nifi.sh run & "
                             r"nifi_pid=$! &&"
                             r"tail -F --pid=${{nifi_pid}} /opt/nifi/nifi-current/logs/nifi-app.log").format(name=name)
        else:
            entry_command = (r"sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' "
                             r"-e 's/^\(nifi.sensitive.props.key\)=.*/\1=secret_key_12345/' "
                             r"-e 's/^\(nifi.remote.input.secure\)=.*/\1=false/' "
                             r"-e 's/^\(nifi.web.http.port\)=.*/\1=8080/' "
                             r"-e 's/^\(nifi.web.https.port\)=.*/\1=/' "
                             r"-e 's/^\(nifi.web.https.host\)=.*/\1=/' "
                             r"-e 's/^\(nifi.web.http.host\)=.*/\1={name}/' "
                             r"-e 's/^\(nifi.security.keystore\)=.*/\1=/' "
                             r"-e 's/^\(nifi.security.keystoreType\)=.*/\1=/' "
                             r"-e 's/^\(nifi.security.keystorePasswd\)=.*/\1=/' "
                             r"-e 's/^\(nifi.security.keyPasswd\)=.*/\1=/' "
                             r"-e 's/^\(nifi.security.truststore\)=.*/\1=/' "
                             r"-e 's/^\(nifi.security.truststoreType\)=.*/\1=/' "
                             r"-e 's/^\(nifi.security.truststorePasswd\)=.*/\1=/' "
                             r"-e 's/^\(nifi.remote.input.socket.port\)=.*/\1=10000/' /opt/nifi/nifi-current/conf/nifi.properties && "
                             r"cp /tmp/nifi_config/flow.json.gz /opt/nifi/nifi-current/conf && /opt/nifi/nifi-current/bin/nifi.sh run & "
                             r"nifi_pid=$! &&"
                             r"tail -F --pid=${{nifi_pid}} /opt/nifi/nifi-current/logs/nifi-app.log").format(name=name)
        if not command:
            command = ["/bin/sh", "-c", entry_command]

        super().__init__("apache/nifi:" + self.NIFI_VERSION, name, test_context.network, entrypoint=command)

    def deploy(self):
        flow_config = self.flow_definition.to_json()
        buffer = io.BytesIO()

        with gzip.GzipFile(fileobj=buffer, mode='wb') as gz_file:
            gz_file.write(flow_config.encode())

        gzipped_bytes = buffer.getvalue()
        self.files.append(File("/tmp/nifi_config/flow.json.gz", gzipped_bytes))

        super().deploy()
        finished_str = "Started Application in"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=300,
            bail_condition=lambda: self.exited,
            context=None)
