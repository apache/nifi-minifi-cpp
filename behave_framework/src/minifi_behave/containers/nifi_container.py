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
import logging
import os
from pathlib import Path

from minifi_behave.containers.file import File
from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.minifi.nifi_flow_definition import NifiFlowDefinition
from minifi_behave.containers.host_file import HostFile
from minifi_behave.core.ssl_utils import make_server_cert

from minifi_behave.core.ssl_utils import dump_cert, dump_key


class NifiContainer(LinuxContainer):
    def __init__(self, test_context: MinifiTestContext, command: list[str] | None = None, use_ssl: bool = False):
        self.flow_definition = NifiFlowDefinition()
        name = f"nifi-{test_context.scenario_id}"
        if use_ssl:
            entry_command = (r"/scripts/convert_cert_to_jks.sh /tmp/resources /tmp/resources/nifi_client.key /tmp/resources/nifi_client.crt /tmp/resources/root_ca.crt &&"
                             r"sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' "
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

        super().__init__("apache/nifi:" + NifiFlowDefinition.NIFI_VERSION, name, test_context.network, entrypoint=command)
        resource_dir = Path(__file__).resolve().parent / "resources" / "nifi"
        self.host_files.append(HostFile("/scripts/convert_cert_to_jks.sh", os.path.join(resource_dir, "convert_cert_to_jks.sh")))

        nifi_client_cert, nifi_client_key = make_server_cert(common_name=f"nifi-{test_context.scenario_id}", ca_cert=test_context.root_ca_cert, ca_key=test_context.root_ca_key)
        self.files.append(File("/tmp/resources/root_ca.crt", dump_cert(test_context.root_ca_cert)))
        self.files.append(File("/tmp/resources/nifi_client.crt", dump_cert(nifi_client_cert)))
        self.files.append(File("/tmp/resources/nifi_client.key", dump_key(nifi_client_key)))

    def deploy(self, context: MinifiTestContext | None) -> bool:
        flow_config = self.flow_definition.to_json()
        logging.info(f"Deploying NiFi container '{self.container_name}' with flow configuration:\n{flow_config}")
        buffer = io.BytesIO()

        with gzip.GzipFile(fileobj=buffer, mode='wb') as gz_file:
            gz_file.write(flow_config.encode())

        gzipped_bytes = buffer.getvalue()
        self.files.append(File("/tmp/nifi_config/flow.json.gz", gzipped_bytes))

        super().deploy(context)
        finished_str = "Started Application in"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=300,
            bail_condition=lambda: self.exited,
            context=context)
