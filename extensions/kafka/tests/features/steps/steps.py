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

import binascii
import time

from behave import step, when, given

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.minifi.processor import Processor
from containers.kafka_server_container import KafkaServer


@step("a Kafka server is set up")
def step_impl(context):
    context.containers["kafka-server"] = KafkaServer(context)


@step("ConsumeKafka processor is set up to communicate with that server")
def step_impl(context):
    consume_kafka = Processor("ConsumeKafka", "ConsumeKafka")
    consume_kafka.add_property("Kafka Brokers", f"kafka-server-{context.scenario_id}:9092")
    consume_kafka.add_property("Topic Names", "ConsumeKafkaTest")
    consume_kafka.add_property("Topic Name Format", "Names")
    consume_kafka.add_property("Honor Transactions", "true")
    consume_kafka.add_property("Group ID", "docker_test_group")
    consume_kafka.add_property("Offset Reset", "earliest")
    consume_kafka.add_property("Key Attribute Encoding", "UTF-8")
    consume_kafka.add_property("Message Header Encoding", "UTF-8")
    consume_kafka.add_property("Max Poll Time", "4 sec")
    consume_kafka.add_property("Session Timeout", "6 sec")
    context.get_or_create_default_minifi_container().flow_definition.add_processor(consume_kafka)


@step("PublishKafka processor is set up to communicate with that server")
def step_impl(context):
    publish_kafka = Processor("PublishKafka", "PublishKafka")
    publish_kafka.add_property("Known Brokers", f"kafka-server-{context.scenario_id}:9092")
    publish_kafka.add_property("Client Name", "minifi-client")
    publish_kafka.add_property("Topic Name", "test")
    publish_kafka.add_property("Batch Size", "10")
    publish_kafka.add_property("Compress Codec", "none")
    publish_kafka.add_property("Delivery Guarantee", "1")
    publish_kafka.add_property("Request Timeout", "10 sec")
    publish_kafka.add_property("Message Timeout", "12 sec")
    context.get_or_create_default_minifi_container().flow_definition.add_processor(publish_kafka)


@step("the Kafka server is started")
def step_impl(context: MinifiTestContext):
    kafka_server_container = context.containers["kafka-server"]
    assert isinstance(kafka_server_container, KafkaServer)
    assert kafka_server_container.deploy()


@step('the topic "{topic_name}" is initialized on the kafka broker')
def step_impl(context: MinifiTestContext, topic_name: str):
    kafka_server_container = context.containers["kafka-server"]
    assert isinstance(kafka_server_container, KafkaServer)
    assert kafka_server_container.create_topic(topic_name=topic_name) or kafka_server_container.log_app_output()


@when('a message with content "{message}" is published to the "{topic_name}" topic')
def step_impl(context: MinifiTestContext, message: str, topic_name: str):
    kafka_server_container = context.containers["kafka-server"]
    assert isinstance(kafka_server_container, KafkaServer)
    assert kafka_server_container.produce_message(topic_name=topic_name, message=message) or kafka_server_container.log_app_output()


@step('a message with content "{message}" is published to the "{topic_name}" topic with key "{key}"')
def step_impl(context: MinifiTestContext, message: str, topic_name: str, key: str):
    kafka_server_container = context.containers["kafka-server"]
    assert isinstance(kafka_server_container, KafkaServer)
    assert kafka_server_container.produce_message_with_key(topic_name=topic_name, message=message, message_key=key) or kafka_server_container.log_app_output()


@given("the \"{property_name}\" property of the {processor_name} processor is set to match {key_attribute_encoding} encoded kafka message key \"{message_key}\"")
def set_property_to_match_message_key(context, property_name, processor_name, key_attribute_encoding, message_key):
    if key_attribute_encoding.lower() == "hex":
        encoded_key = binascii.hexlify(message_key.encode("utf-8")).upper()
    elif key_attribute_encoding.lower() == "(not set)":
        encoded_key = message_key.encode("utf-8")
    else:
        encoded_key = message_key.encode(key_attribute_encoding)
    filtering = "${kafka.key:equals('" + encoded_key.decode("utf-8") + "')}"
    processor = context.get_or_create_default_minifi_container().flow_definition.get_processor(processor_name)
    processor.add_property(property_name, filtering)


@when("the publisher performs a {transaction_type} transaction publishing to the \"{topic_name}\" topic these messages: {messages}")
def publish_to_topic_transaction_style(context, transaction_type, topic_name, messages):
    if transaction_type == "SINGLE_COMMITTED_TRANSACTION":
        python_code = f"""
from confluent_kafka import Producer

producer = Producer({{"bootstrap.servers": "kafka-server-{context.scenario_id}:9092", "transactional.id": "1001"}})
producer.init_transactions()
producer.begin_transaction()
for content in "{messages}".split(", "):
    producer.produce("{topic_name}", content.encode("utf-8"))
producer.commit_transaction()
producer.flush(10)
        """
    elif transaction_type == "TWO_SEPARATE_TRANSACTIONS":
        python_code = f"""
from confluent_kafka import Producer

producer = Producer({{"bootstrap.servers": "kafka-server-{context.scenario_id}:9092", "transactional.id": "1001"}})
producer.init_transactions()
for content in "{messages}".split(", "):
    producer.begin_transaction()
    producer.produce("{topic_name}", content.encode("utf-8"))
    producer.commit_transaction()
producer.flush(10)
        """
    elif transaction_type == "NON_COMMITTED_TRANSACTION":
        python_code = f"""
from confluent_kafka import Producer

producer = Producer({{"bootstrap.servers": "kafka-server-{context.scenario_id}:9092", "transactional.id": "1001"}})
producer.init_transactions()
producer.begin_transaction()
for content in "{messages}".split(", "):
    producer.produce("{topic_name}", content.encode("utf-8"))
producer.flush(10)
        """
    elif transaction_type == "CANCELLED_TRANSACTION":
        python_code = f"""
from confluent_kafka import Producer

producer = Producer({{"bootstrap.servers": "kafka-server-{context.scenario_id}:9092", "transactional.id": "1001"}})
producer.init_transactions()
producer.begin_transaction()
for content in "{messages}".split(", "):
    producer.produce("{topic_name}", content.encode("utf-8"))
producer.flush(10)
producer.abort_transaction()
        """
    else:
        raise Exception("Unknown transaction type.")
    assert context.containers["kafka-server"].run_python_in_kafka_helper_docker(python_code) or context.containers["kafka-server"].log_app_output()


@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic with headers \"{semicolon_separated_headers}\"")
def publish_with_headers_to_topic(context, content, topic_name, semicolon_separated_headers):
    python_code = f"""
from confluent_kafka import Producer
headers = []
for header in "{semicolon_separated_headers}".split(";"):
    kv = header.split(":")
    headers.append((kv[0].strip(), kv[1].strip().encode("utf-8")))
producer = Producer({{"bootstrap.servers": "kafka-server-{context.scenario_id}:9092"}})
producer.produce("{topic_name}", "{content}".encode("utf-8"), headers=headers)
producer.flush(10)
    """
    assert context.containers["kafka-server"].run_python_in_kafka_helper_docker(python_code) or context.containers["kafka-server"].log_app_output()


@when("two messages with content \"{content_one}\" and \"{content_two}\" is published to the \"{topic_name}\" topic")
def publish_two_messages_to_topic(context, content_one, content_two, topic_name):
    python_code = f"""
from confluent_kafka import Producer
import uuid
producer = Producer({{"bootstrap.servers": "kafka-server-{context.scenario_id}:9092"}})
producer.produce("{topic_name}", "{content_one}")
producer.produce("{topic_name}", "{content_two}")
producer.flush(10)
    """
    assert context.containers["kafka-server"].run_python_in_kafka_helper_docker(python_code) or context.containers["kafka-server"].log_app_output()


@when("{number_of_messages} kafka messages are sent to the topic \"{topic_name}\"")
def publish_batch_to_topic(context, number_of_messages, topic_name):
    python_code = f"""
from confluent_kafka import Producer
import uuid
producer = Producer({{"bootstrap.servers": "kafka-server-{context.scenario_id}:9092"}})
for i in range(0, int({number_of_messages})):
    producer.produce("{topic_name}", str(uuid.uuid4()).encode("utf-8"))
producer.flush(10)
    """
    assert context.containers["kafka-server"].run_python_in_kafka_helper_docker(python_code) or context.containers["kafka-server"].log_app_output()


@when("the Kafka consumer is registered in kafka broker")
def wait_for_consumer_registration(context):
    assert context.containers["kafka-server"].wait_for_kafka_consumer_to_be_registered(1) or context.containers["kafka-server"].log_app_output()
    # After the consumer is registered there is still some time needed for consumer-broker synchronization
    # Unfortunately there are no additional log messages that could be checked for this
    time.sleep(2)


@when("the Kafka consumer is reregistered in kafka broker")
def wait_for_consumer_registration(context):
    assert context.containers["kafka-server"].wait_for_kafka_consumer_to_be_registered(2) or context.containers["kafka-server"].log_app_output()
    # After the consumer is registered there is still some time needed for consumer-broker synchronization
    # Unfortunately there are no additional log messages that could be checked for this
    time.sleep(2)
