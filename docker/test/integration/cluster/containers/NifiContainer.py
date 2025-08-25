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

from .FlowContainer import FlowContainer
from minifi.flow_serialization.Nifi_flow_json_serializer import Nifi_flow_json_serializer
import gzip
import os


class NiFiOptions:
    def __init__(self):
        self.use_ssl = False


class NifiContainer(FlowContainer):
    NIFI_VERSION = '2.2.0'
    NIFI_ROOT = '/opt/nifi/nifi-' + NIFI_VERSION

    def __init__(self, feature_context, config_dir, options, name, vols, network, image_store, command=None):
        if not command:
            if options.use_ssl:
                entry_command = (r"sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.remote.input.secure\)=.*/\1=true/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.sensitive.props.key\)=.*/\1=secret_key_12345/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.web.https.port\)=.*/\1=8443/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.web.https.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keystore\)=.*/\1=\/tmp\/resources\/keystore.jks/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keystoreType\)=.*/\1=jks/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keystorePasswd\)=.*/\1=passw0rd1!/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keyPasswd\)=.*/#\1=passw0rd1!/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.truststore\)=.*/\1=\/tmp\/resources\/truststore.jks/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.truststoreType\)=.*/\1=jks/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.truststorePasswd\)=.*/\1=passw0rd1!/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.remote.input.socket.port\)=.*/\1=10443/' {nifi_root}/conf/nifi.properties && "
                                 r"cp /tmp/nifi_config/flow.json.gz {nifi_root}/conf && {nifi_root}/bin/nifi.sh run & "
                                 r"nifi_pid=$! &&"
                                 r"tail -F --pid=${{nifi_pid}} {nifi_root}/logs/nifi-app.log").format(name=name, nifi_root=NifiContainer.NIFI_ROOT)
            else:
                entry_command = (r"sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.sensitive.props.key\)=.*/\1=secret_key_12345/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.remote.input.secure\)=.*/\1=false/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.web.http.port\)=.*/\1=8080/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.web.https.port\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.web.https.host\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.web.http.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keystore\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keystoreType\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keystorePasswd\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.keyPasswd\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.truststore\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.truststoreType\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.security.truststorePasswd\)=.*/\1=/' {nifi_root}/conf/nifi.properties && "
                                 r"sed -i -e 's/^\(nifi.remote.input.socket.port\)=.*/\1=10000/' {nifi_root}/conf/nifi.properties && "
                                 r"cp /tmp/nifi_config/flow.json.gz {nifi_root}/conf && {nifi_root}/bin/nifi.sh run & "
                                 r"nifi_pid=$! &&"
                                 r"tail -F --pid=${{nifi_pid}} {nifi_root}/logs/nifi-app.log").format(name=name, nifi_root=NifiContainer.NIFI_ROOT)
            command = ["/bin/sh", "-c", entry_command]
        super().__init__(feature_context, config_dir, name, 'nifi', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Started Application in"

    def __create_config(self):
        serializer = Nifi_flow_json_serializer()
        test_flow_json = serializer.serialize(self.start_nodes, NifiContainer.NIFI_VERSION)
        logging.info('Using generated flow config json:\n%s', test_flow_json)

        with gzip.open(os.path.join(self.config_dir, "flow.json.gz"), 'wb') as gz_file:
            gz_file.write(test_flow_json.encode())

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running nifi docker container...')
        self.__create_config()
        self.client.containers.run(
            "apache/nifi:" + NifiContainer.NIFI_VERSION,
            detach=True,
            name=self.name,
            hostname=self.name,
            network=self.network.name,
            entrypoint=self.command,
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
