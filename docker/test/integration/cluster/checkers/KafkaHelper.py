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
from __future__ import annotations

import io
import logging
import docker


class KafkaHelper:
    def __init__(self, container_communicator, feature_id):
        self.container_communicator = container_communicator
        self.feature_id = feature_id

    def create_topic(self, container_name: str, topic_name: str):
        logging.info(f"Creating topic {topic_name} in {container_name}")
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c", f"/opt/bitnami/kafka/bin/kafka-topics.sh --create --topic {topic_name} --bootstrap-server {container_name}:9092"])
        logging.info(output)
        return code == 0

    def produce_message(self, container_name: str, topic_name: str, message: str):
        logging.info(f"Sending {message} to {container_name}:{topic_name}")
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c", f"/opt/bitnami/kafka/bin/kafka-console-producer.sh --topic {topic_name} --bootstrap-server {container_name}:9092 <<< '{message}'"])
        logging.info(output)
        return code == 0

    def produce_message_with_key(self, container_name: str, topic_name: str, message: str, message_key: str):
        logging.info(f"Sending {message} to {container_name}:{topic_name}")
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c", f"/opt/bitnami/kafka/bin/kafka-console-producer.sh --property 'key.separator=:' --property 'parse.key=true' --topic {topic_name} --bootstrap-server {container_name}:9092 <<< '{message_key}:{message}'"])
        logging.info(output)
        return code == 0

    def run_python_in_kafka_helper_docker(self, command: str):
        try:
            self.container_communicator.client.images.get("kafka-helper")
        except docker.errors.ImageNotFound:
            dockerfile_content = """
            FROM python:3.13-slim-bookworm
            RUN pip install confluent-kafka
            """
            dockerfile_stream = io.BytesIO(dockerfile_content.encode("utf-8"))
            image, _ = self.container_communicator.client.images.build(fileobj=dockerfile_stream, tag="kafka-helper")

        output = self.container_communicator.client.containers.run("kafka-helper", ["python", "-c", command], remove=True, stdout=True, stderr=True, network=f"minifi_integration_test_network-{self.feature_id}")
        logging.info(output)
        return True
