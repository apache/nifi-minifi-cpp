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
from minifi.flow_serialization.Nifi_flow_xml_serializer import Nifi_flow_xml_serializer
import gzip
import os


class NifiContainer(FlowContainer):
    NIFI_VERSION = '1.20.0'
    NIFI_ROOT = '/opt/nifi/nifi-' + NIFI_VERSION

    def __init__(self, config_dir, name, vols, network, image_store, command=None):
        if not command:
            entry_command = (r"sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties && "
                             r"sed -i -e 's/^\(nifi.sensitive.props.key\)=.*/\1=secret_key_12345/' {nifi_root}/conf/nifi.properties && "
                             r"cp /tmp/nifi_config/flow.xml.gz {nifi_root}/conf && /opt/nifi/scripts/start.sh").format(name=name, nifi_root=NifiContainer.NIFI_ROOT)
            command = ["/bin/sh", "-c", entry_command]
        super().__init__(config_dir, name, 'nifi', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Starting Flow Controller"

    def __create_config(self):
        serializer = Nifi_flow_xml_serializer()
        test_flow_xml = serializer.serialize(self.start_nodes, NifiContainer.NIFI_VERSION)
        logging.info('Using generated flow config xml:\n%s', test_flow_xml)

        with gzip.open(os.path.join(self.config_dir, "flow.xml.gz"), 'wb') as gz_file:
            gz_file.write(test_flow_xml.encode())

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
            environment=["NIFI_WEB_HTTP_PORT=8080"],
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
