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
import tempfile
import os
import docker.types
import jks
from OpenSSL import crypto

from .Container import Container
from ssl_utils.SSL_cert_utils import make_server_cert


class KafkaBrokerContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None):
        super().__init__(feature_context, name, 'kafka-broker', vols, network, image_store, command)

        kafka_cert, kafka_key = make_server_cert(f"kafka-broker-{feature_context.id}", feature_context.root_ca_cert, feature_context.root_ca_key)

        pke = jks.PrivateKeyEntry.new('kafka-broker-cert', [crypto.dump_certificate(crypto.FILETYPE_ASN1, kafka_cert)], crypto.dump_privatekey(crypto.FILETYPE_ASN1, kafka_key), 'rsa_raw')
        server_keystore = jks.KeyStore.new('jks', [pke])

        self.server_keystore_file = tempfile.NamedTemporaryFile(delete=False)
        server_keystore.save(self.server_keystore_file.name, 'abcdefgh')
        self.server_keystore_file.close()

        self.server_truststore_file = tempfile.NamedTemporaryFile(delete=False)
        self.server_truststore_file.write(crypto.dump_certificate(crypto.FILETYPE_PEM, feature_context.root_ca_cert))
        self.server_truststore_file.close()

        self.server_properties_file = tempfile.NamedTemporaryFile(delete=False)
        self.feature_id = feature_context.id
        with open(os.environ['TEST_DIRECTORY'] + "/resources/kafka_broker/conf/server.properties") as server_properties_file:
            server_properties_content = server_properties_file.read()
            patched_server_properties_content = server_properties_content.replace("kafka-broker", f"kafka-broker-{feature_context.id}")
            self.server_properties_file.write(patched_server_properties_content.encode())
            self.server_properties_file.close()
            os.chmod(self.server_properties_file.name, 0o644)

    def get_startup_finished_log_entry(self):
        return "Recorded new controller, from now on will use broker kafka-broker"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running kafka broker docker container...')

        self.client.containers.run(
            image="ubuntu/kafka:3.1-22.04_beta",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'9092/tcp': 9092, '29092/tcp': 29092, '9093/tcp': 9093, '29093/tcp': 29093, '9094/tcp': 9094, '29094/tcp': 29094, '9094/tcp': 9094, '29095/tcp': 29095},
            environment=[
                "ZOOKEEPER_HOST=zookeeper-" + self.feature_id,
                "ZOOKEEPER_PORT=2181"
            ],
            mounts=[
                docker.types.Mount(
                    type='bind',
                    source=self.server_properties_file.name,
                    target='/opt/kafka/config/kraft/server.properties'
                ),
                docker.types.Mount(
                    type='bind',
                    source=self.server_properties_file.name,
                    target='/opt/kafka/config/server.properties'
                ),
                docker.types.Mount(
                    type='bind',
                    source=self.server_keystore_file.name,
                    target='/usr/local/etc/kafka/certs/server_keystore.jks'
                ),
                docker.types.Mount(
                    type='bind',
                    source=self.server_truststore_file.name,
                    target='/usr/local/etc/kafka/certs/server_truststore.pem'
                )
            ],
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
