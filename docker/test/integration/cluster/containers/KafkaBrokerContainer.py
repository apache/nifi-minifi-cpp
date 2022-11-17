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
            environment=[
                "KAFKA_BROKER_ID=1",
                "ALLOW_PLAINTEXT_LISTENER=yes",
                "KAFKA_AUTO_CREATE_TOPICS_ENABLE=true",
                "KAFKA_LISTENERS=PLAINTEXT://kafka-broker:9092,SSL://kafka-broker:9093,SASL_PLAINTEXT://kafka-broker:9094,SASL_SSL://kafka-broker:9095,SSL_HOST://0.0.0.0:29093,PLAINTEXT_HOST://0.0.0.0:29092,SASL_PLAINTEXT_HOST://0.0.0.0:29094,SASL_SSL_HOST://0.0.0.0:29095",
                "KAFKA_LISTENER_SECURITY_PROTOCOL_MAP=PLAINTEXT:PLAINTEXT,PLAINTEXT_HOST:PLAINTEXT,SSL:SSL,SSL_HOST:SSL,SASL_PLAINTEXT:SASL_PLAINTEXT,SASL_PLAINTEXT_HOST:SASL_PLAINTEXT,SASL_SSL:SASL_SSL,SASL_SSL_HOST:SASL_SSL",
                "KAFKA_SECURITY_INTER_BROKER_PROTOCOL=SSL",
                "KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://kafka-broker:9092,PLAINTEXT_HOST://localhost:29092,SSL://kafka-broker:9093,SSL_HOST://localhost:29093,SASL_PLAINTEXT://kafka-broker:9094,SASL_PLAINTEXT_HOST://localhost:29094,SASL_SSL://kafka-broker:9095,SASL_SSL_HOST://localhost:29095",
                "KAFKA_HEAP_OPTS=-Xms512m -Xmx1g",
                "KAFKA_ZOOKEEPER_CONNECT=zookeeper:2181",
                "SSL_CLIENT_AUTH=none"],
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
