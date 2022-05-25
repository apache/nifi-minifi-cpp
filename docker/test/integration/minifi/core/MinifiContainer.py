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


import os
import logging
from .FlowContainer import FlowContainer
from ..flow_serialization.Minifi_flow_yaml_serializer import Minifi_flow_yaml_serializer


class MinifiContainer(FlowContainer):
    MINIFI_VERSION = os.environ['MINIFI_VERSION']
    MINIFI_ROOT = '/opt/minifi/nifi-minifi-cpp-' + MINIFI_VERSION

    def __init__(self, config_dir, name, vols, network, image_store, command=None, engine='minifi-cpp'):
        if not command:
            command = ["/bin/sh", "-c", "cp /tmp/minifi_config/config.yml " + MinifiContainer.MINIFI_ROOT + "/conf && /opt/minifi/minifi-current/bin/minifi.sh run"]
        super().__init__(config_dir, name, engine, vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Starting Flow Controller"

    def _create_config(self):
        serializer = Minifi_flow_yaml_serializer()
        test_flow_yaml = serializer.serialize(self.start_nodes)
        logging.info('Using generated flow config yml:\n%s', test_flow_yaml)
        with open(os.path.join(self.config_dir, "config.yml"), 'wb') as config_file:
            config_file.write(test_flow_yaml.encode('utf-8'))

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running minifi docker container...')
        self._create_config()

        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'9100/tcp': 9100},
            entrypoint=self.command,
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
