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
from .Container import Container


class MqttBrokerContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None):
        super().__init__(feature_context, name, 'mqtt-broker', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "mosquitto version [0-9\\.]+ running"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running MQTT broker docker container...')
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            ports={'1883/tcp': 1883},
            network=self.network.name,
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
