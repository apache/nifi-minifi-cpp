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

        with tempfile.NamedTemporaryFile(delete=False, suffix=".jks") as server_keystore_file:
            server_keystore.save(server_keystore_file.name, 'abcdefgh')
            self.server_keystore_file_path = server_keystore_file.name

        trusted_cert = jks.TrustedCertEntry.new(
            'root-ca',  # Alias for the certificate
            crypto.dump_certificate(crypto.FILETYPE_ASN1, feature_context.root_ca_cert)
        )

        # Create a JKS keystore that will serve as a truststore with the trusted cert entry.
        truststore = jks.KeyStore.new('jks', [trusted_cert])

        with tempfile.NamedTemporaryFile(delete=False, suffix=".jks") as server_truststore_file:
            truststore.save(server_truststore_file.name, 'abcdefgh')
            self.server_truststore_file_path = server_truststore_file.name

        with tempfile.NamedTemporaryFile(delete=False, mode="w", encoding="utf-8") as jaas_config_file:
            jaas_config_file.write("""
KafkaServer {
  org.apache.kafka.common.security.plain.PlainLoginModule required
  username="admin"
  password="admin-secret"
  user_admin="admin-secret"
  user_alice="alice-secret";
};

Client {
  org.apache.kafka.common.security.plain.PlainLoginModule required
  username="admin"
  password="admin-secret";
};
""")
            self.jaas_config_file_path = jaas_config_file.name

    def get_startup_finished_log_entry(self):
        return "Kafka Server started"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running kafka broker docker container...')
        self.client.containers.run(
            image="bitnami/kafka:3.9.0",
            detach=True,
            name=self.name,
            network=self.network.name,
            environment=[
                "KAFKA_CFG_NODE_ID=1",
                "KAFKA_CFG_PROCESS_ROLES=controller,broker",
                "KAFKA_CFG_INTER_BROKER_LISTENER_NAME=PLAINTEXT",
                "KAFKA_CFG_CONTROLLER_LISTENER_NAMES=CONTROLLER",

                f"KAFKA_CFG_CONTROLLER_QUORUM_VOTERS=1@kafka-broker-{self.feature_context.id}:9096",

                f"KAFKA_CFG_LISTENERS=PLAINTEXT://kafka-broker-{self.feature_context.id}:9092,"
                f"SASL_PLAINTEXT://kafka-broker-{self.feature_context.id}:9094,"
                f"SSL://kafka-broker-{self.feature_context.id}:9093,"
                f"SASL_SSL://kafka-broker-{self.feature_context.id}:9095,"
                f"CONTROLLER://kafka-broker-{self.feature_context.id}:9096",

                f"KAFKA_CFG_ADVERTISED_LISTENERS=PLAINTEXT://kafka-broker-{self.feature_context.id}:9092,"
                f"SASL_PLAINTEXT://kafka-broker-{self.feature_context.id}:9094,"
                f"SSL://kafka-broker-{self.feature_context.id}:9093,"
                f"SASL_SSL://kafka-broker-{self.feature_context.id}:9095,"
                f"CONTROLLER://kafka-broker-{self.feature_context.id}:9096",

                "KAFKA_CFG_LOG4J_ROOT_LOGLEVEL=DEBUG",
                "KAFKA_CFG_LOG4J_LOGGERS=kafka.controller=DEBUG,kafka.server.KafkaApis=DEBUG",

                "KAFKA_CFG_LISTENER_SECURITY_PROTOCOL_MAP=PLAINTEXT:PLAINTEXT,"
                "SASL_PLAINTEXT:SASL_PLAINTEXT,"
                "SSL:SSL,"
                "SASL_SSL:SASL_SSL,"
                "CONTROLLER:PLAINTEXT",

                # **If using SASL_PLAINTEXT, provide JAAS config**
                'KAFKA_CFG_SASL_MECHANISM_INTER_BROKER_PROTOCOL=PLAIN',
                'KAFKA_CFG_SASL_ENABLED_MECHANISMS=PLAIN',
                'KAFKA_OPTS=-Djava.security.auth.login.config=/opt/bitnami/kafka/config/kafka_jaas.conf',

                "KAFKA_CFG_SSL_PROTOCOL=TLS",
                "KAFKA_CFG_SSL_ENABLED_PROTOCOLS=TLSv1.2",
                "KAFKA_CFG_SSL_KEYSTORE_TYPE=JKS",
                "KAFKA_CFG_SSL_KEYSTORE_LOCATION=/bitnami/kafka/config/certs/kafka.keystore.jks",
                "KAFKA_CFG_SSL_KEYSTORE_PASSWORD=abcdefgh",
                "KAFKA_CFG_SSL_KEY_PASSWORD=abcdefgh",
                "KAFKA_CFG_SSL_TRUSTSTORE_TYPE=JKS",
                "KAFKA_CFG_SSL_TRUSTSTORE_LOCATION=/bitnami/kafka/config/certs/kafka.truststore.jks",
                "KAFKA_CFG_SSL_CLIENT_AUTH=none"
            ],
            mounts=[
                docker.types.Mount(
                    type='bind',
                    source=self.server_keystore_file_path,
                    target='/bitnami/kafka/config/certs/kafka.keystore.jks'
                ),
                docker.types.Mount(
                    type='bind',
                    source=self.server_truststore_file_path,
                    target='/bitnami/kafka/config/certs/kafka.truststore.jks'
                ),
                docker.types.Mount(
                    type='bind',
                    source=self.jaas_config_file_path,
                    target='/opt/bitnami/kafka/config/kafka_jaas.conf'
                )
            ],
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
