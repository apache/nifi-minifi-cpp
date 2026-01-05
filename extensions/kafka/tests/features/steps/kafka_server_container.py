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
import re
import jks

from OpenSSL import crypto
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.ssl_utils import make_server_cert
from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.file import File
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class KafkaServer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("apache/kafka:4.1.0", f"kafka-server-{test_context.scenario_id}", test_context.network)
        self.environment.append("KAFKA_NODE_ID=1")
        self.environment.append("KAFKA_PROCESS_ROLES=controller,broker")
        self.environment.append("KAFKA_INTER_BROKER_LISTENER_NAME=PLAINTEXT")
        self.environment.append("KAFKA_CONTROLLER_LISTENER_NAMES=CONTROLLER")
        self.environment.append("KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR=1")
        self.environment.append("KAFKA_TRANSACTION_STATE_LOG_REPLICATION_FACTOR=1")
        self.environment.append("KAFKA_TRANSACTION_STATE_LOG_MIN_ISR=1")
        self.environment.append(f"KAFKA_CONTROLLER_QUORUM_VOTERS=1@kafka-server-{test_context.scenario_id}:9096")
        self.environment.append(f"KAFKA_LISTENERS=PLAINTEXT://kafka-server-{test_context.scenario_id}:9092,SASL_PLAINTEXT://kafka-server-{test_context.scenario_id}:9094,SSL://kafka-server-{test_context.scenario_id}:9093,SASL_SSL://kafka-server-{test_context.scenario_id}:9095,CONTROLLER://kafka-server-{test_context.scenario_id}:9096")
        self.environment.append(f"KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://kafka-server-{test_context.scenario_id}:9092,SASL_PLAINTEXT://kafka-server-{test_context.scenario_id}:9094,SSL://kafka-server-{test_context.scenario_id}:9093,SASL_SSL://kafka-server-{test_context.scenario_id}:9095,CONTROLLER://kafka-server-{test_context.scenario_id}:9096")
        self.environment.append("KAFKA_LISTENER_SECURITY_PROTOCOL_MAP=PLAINTEXT:PLAINTEXT,SASL_PLAINTEXT:SASL_PLAINTEXT,SSL:SSL,SASL_SSL:SASL_SSL,CONTROLLER:PLAINTEXT")
        self.environment.append("KAFKA_SASL_MECHANISM_INTER_BROKER_PROTOCOL=PLAIN")
        self.environment.append("KAFKA_SASL_ENABLED_MECHANISMS=PLAIN")
        self.environment.append("KAFKA_OPTS=-Djava.security.auth.login.config=/opt/kafka/config/kafka_jaas.conf -Dlog4j2.rootLogger.level=DEBUG -Dlog4j2.logger.org.apache.kafka.controller.level=DEBUG")
        self.environment.append("KAFKA_SSL_PROTOCOL=TLS")
        self.environment.append("KAFKA_SSL_ENABLED_PROTOCOLS=TLSv1.2")
        self.environment.append("KAFKA_SSL_KEYSTORE_TYPE=JKS")
        self.environment.append("KAFKA_SSL_KEYSTORE_FILENAME=kafka.keystore.jks")
        self.environment.append("KAFKA_SSL_KEYSTORE_CREDENTIALS=credentials.conf")
        self.environment.append("KAFKA_SSL_KEY_CREDENTIALS=credentials.conf")
        self.environment.append("KAFKA_SSL_TRUSTSTORE_CREDENTIALS=credentials.conf")
        self.environment.append("KAFKA_SSL_TRUSTSTORE_TYPE=JKS")
        self.environment.append("KAFKA_SSL_TRUSTSTORE_FILENAME=kafka.truststore.jks")
        self.environment.append("KAFKA_SSL_CLIENT_AUTH=none")

        kafka_cert, kafka_key = make_server_cert(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)

        pke = jks.PrivateKeyEntry.new('kafka-broker-cert', [crypto.dump_certificate(crypto.FILETYPE_ASN1, kafka_cert)], crypto.dump_privatekey(crypto.FILETYPE_ASN1, kafka_key), 'rsa_raw')
        server_keystore = jks.KeyStore.new('jks', [pke])
        server_keystore_content = server_keystore.saves('abcdefgh')
        self.files.append(File("/etc/kafka/secrets/kafka.keystore.jks", server_keystore_content, permissions=0o644))
        self.files.append(File("/etc/kafka/secrets/credentials.conf", b'abcdefgh', permissions=0o644))

        trusted_cert = jks.TrustedCertEntry.new(
            'root-ca',  # Alias for the certificate
            crypto.dump_certificate(crypto.FILETYPE_ASN1, test_context.root_ca_cert)
        )

        # Create a JKS keystore that will serve as a truststore with the trusted cert entry.
        truststore = jks.KeyStore.new('jks', [trusted_cert])
        truststore_content = truststore.saves('abcdefgh')
        self.files.append(File("/etc/kafka/secrets/kafka.truststore.jks", truststore_content, permissions=0o644))

        jaas_config_file_content = """
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
"""
        self.files.append(File("/opt/kafka/config/kafka_jaas.conf", jaas_config_file_content, permissions=0o644))

    def deploy(self):
        super().deploy()
        finished_str = "Kafka Server started"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=60,
            bail_condition=lambda: self.exited,
            context=None)

    def create_topic(self, topic_name: str):
        (code, output) = self.exec_run(["/bin/sh", "-c", f"/opt/kafka/bin/kafka-topics.sh --create --topic '{topic_name}' --bootstrap-server '{self.container_name}':9092"])
        logging.info(f"Create topic output: '{output}' with code {code}")
        return code == 0

    def produce_message(self, topic_name: str, message: str):
        (code, output) = self.exec_run(["/bin/sh", "-c", f"echo '{message}' | /opt/kafka/bin/kafka-console-producer.sh --topic '{topic_name}' --bootstrap-server '{self.container_name}':9092"])
        logging.info(f"Produce message output: '{output}' with code {code}")
        return code == 0

    def produce_message_with_key(self, topic_name: str, message: str, message_key: str):
        (code, output) = self.exec_run(["/bin/sh", "-c", f" echo '{message_key}:{message}' | /opt/kafka/bin/kafka-console-producer.sh --property 'key.separator=:' --property 'parse.key=true' --topic '{topic_name}' --bootstrap-server '{self.container_name}':9092"])
        logging.info(f"Produce message with key output: '{output}' with code {code}")
        return code == 0

    def run_python_in_kafka_helper_docker(self, command: str):
        output = self.client.containers.run("minifi-kafka-helper:latest", ["python", "-c", command], remove=True, stdout=True, stderr=True, network=self.network.name)
        logging.info(f"Run python in kafka helper docker output: '{output.decode('utf-8')}'")
        return True

    def wait_for_kafka_consumer_to_be_registered(self, expected_consumer_count: int):
        message_regex = "Assignment received from leader.*for group docker_test_group"
        return wait_for_condition(
            condition=lambda: len(re.findall(message_regex, self.get_logs())) >= expected_consumer_count,
            timeout_seconds=60,
            bail_condition=lambda: self.exited,
            context=None)
