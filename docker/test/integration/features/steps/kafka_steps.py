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


from behave import given, when
import logging
import binascii
import time


@given("a kafka producer workflow publishing files placed in \"{directory}\" to a broker exactly once")
def publish_producer_workflow(context, directory):
    context.execute_steps("""
        given a GetFile processor with the \"Input Directory\" property set to \"{directory}\"
        and the \"Keep Source File\" property of the GetFile processor is set to \"false\"
        and a PublishKafka processor set up to communicate with a kafka broker instance
        and the "success" relationship of the GetFile processor is connected to the PublishKafka""".format(directory=directory))


@given("the \"{property_name}\" property of the {processor_name} processor is set to match {key_attribute_encoding} encoded kafka message key \"{message_key}\"")
def set_property_to_match_message_key(context, property_name, processor_name, key_attribute_encoding, message_key):
    encoded_key = ""
    if (key_attribute_encoding.lower() == "hex"):
        # Hex is presented upper-case to be in sync with NiFi
        encoded_key = binascii.hexlify(message_key.encode("utf-8")).upper()
    elif (key_attribute_encoding.lower() == "(not set)"):
        encoded_key = message_key.encode("utf-8")
    else:
        encoded_key = message_key.encode(key_attribute_encoding)
    logging.info("%s processor is set up to match encoded key \"%s\"", processor_name, encoded_key)
    filtering = "${kafka.key:equals('" + encoded_key.decode("utf-8") + "')}"
    logging.info("Filter: \"%s\"", filtering)
    processor = context.test.get_node_by_name(processor_name)
    processor.set_property(property_name, filtering)

    # Kafka setup


@given("a kafka broker is set up in correspondence with the PublishKafka")
@given("a kafka broker is set up in correspondence with the third-party kafka publisher")
@given("a kafka broker is set up in correspondence with the publisher flow")
def kafka_broker_setup(context):
    context.test.acquire_container(context=context, name="kafka-broker", engine="kafka-broker")


@given("the kafka broker is started")
def kafka_broker_start(context):
    context.test.start_kafka_broker(context)


@given("the topic \"{topic_name}\" is initialized on the kafka broker")
def init_topic(context, topic_name):
    container_name = context.test.get_container_name_with_postfix("kafka-broker")
    assert context.test.cluster.kafka_checker.create_topic(container_name, topic_name) or context.test.cluster.log_app_output()


@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic")
def publish_to_topic(context, content, topic_name):
    container_name = context.test.get_container_name_with_postfix("kafka-broker")
    assert context.test.cluster.kafka_checker.produce_message(container_name, topic_name, content) or context.test.cluster.log_app_output()


@when("the publisher performs a {transaction_type} transaction publishing to the \"{topic_name}\" topic these messages: {messages}")
def publish_to_topic_transaction_style(context, transaction_type, topic_name, messages):
    if transaction_type == "SINGLE_COMMITTED_TRANSACTION":
        python_code = f"""
from confluent_kafka import Producer

producer = Producer({{"bootstrap.servers": "kafka-broker-{context.feature_id}:9092", "transactional.id": "1001"}})
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

producer = Producer({{"bootstrap.servers": "kafka-broker-{context.feature_id}:9092", "transactional.id": "1001"}})
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

producer = Producer({{"bootstrap.servers": "kafka-broker-{context.feature_id}:9092", "transactional.id": "1001"}})
producer.init_transactions()
producer.begin_transaction()
for content in "{messages}".split(", "):
    producer.produce("{topic_name}", content.encode("utf-8"))
producer.flush(10)
        """
    elif transaction_type == "CANCELLED_TRANSACTION":
        python_code = f"""
from confluent_kafka import Producer

producer = Producer({{"bootstrap.servers": "kafka-broker-{context.feature_id}:9092", "transactional.id": "1001"}})
producer.init_transactions()
producer.begin_transaction()
for content in "{messages}".split(", "):
    producer.produce("{topic_name}", content.encode("utf-8"))
producer.flush(10)
producer.abort_transaction()
        """
    else:
        raise Exception("Unknown transaction type.")
    assert context.test.cluster.kafka_checker.run_python_in_kafka_helper_docker(python_code) or context.test.cluster.log_app_output()


@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic with key \"{message_key}\"")
def publish_to_topic_with_key(context, content, topic_name, message_key):
    container_name = context.test.get_container_name_with_postfix("kafka-broker")
    assert context.test.cluster.kafka_checker.produce_message_with_key(container_name, topic_name, content, message_key) or context.test.cluster.log_app_output()


@when("{number_of_messages} kafka messages are sent to the topic \"{topic_name}\"")
def publish_batch_to_topic(context, number_of_messages, topic_name):
    python_code = f"""
from confluent_kafka import Producer
import uuid
producer = Producer({{"bootstrap.servers": "kafka-broker-{context.feature_id}:9092"}})
for i in range(0, int({number_of_messages})):
    producer.produce("{topic_name}", str(uuid.uuid4()).encode("utf-8"))
producer.flush(10)
    """
    assert context.test.cluster.kafka_checker.run_python_in_kafka_helper_docker(python_code) or context.test.cluster.log_app_output()


@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic with headers \"{semicolon_separated_headers}\"")
def publish_with_headers_to_topic(context, content, topic_name, semicolon_separated_headers):
    python_code = f"""
from confluent_kafka import Producer
headers = []
for header in "{semicolon_separated_headers}".split(";"):
    kv = header.split(":")
    headers.append((kv[0].strip(), kv[1].strip().encode("utf-8")))
producer = Producer({{"bootstrap.servers": "kafka-broker-{context.feature_id}:9092"}})
producer.produce("{topic_name}", "{content}".encode("utf-8"), headers=headers)
producer.flush(10)
    """
    assert context.test.cluster.kafka_checker.run_python_in_kafka_helper_docker(python_code) or context.test.cluster.log_app_output()


@when("the Kafka consumer is reregistered in kafka broker")
def wait_for_consumer_reregistration(context):
    context.test.wait_for_kafka_consumer_to_be_registered("kafka-broker", 2)
    # After the consumer is registered there is still some time needed for consumer-broker synchronization
    # Unfortunately there are no additional log messages that could be checked for this
    time.sleep(2)


@when("the Kafka consumer is registered in kafka broker")
def wait_for_consumer_registration(context):
    context.test.wait_for_kafka_consumer_to_be_registered("kafka-broker", 1)
    # After the consumer is registered there is still some time needed for consumer-broker synchronization
    # Unfortunately there are no additional log messages that could be checked for this
    time.sleep(2)
