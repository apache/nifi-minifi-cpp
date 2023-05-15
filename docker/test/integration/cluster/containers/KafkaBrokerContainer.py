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


class KafkaBrokerContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'kafka-broker', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Kafka startTimeMs"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running kafka broker docker container...')
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name='kafka-broker',
            network=self.network.name,
            ports={'9092/tcp': 9092, '29092/tcp': 29092, '9093/tcp': 9093, '29093/tcp': 29093, '9094/tcp': 9094, '29094/tcp': 29094, '9094/tcp': 9094, '29095/tcp': 29095},
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
