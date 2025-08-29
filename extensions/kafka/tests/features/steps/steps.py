import binascii

from behave import step, when, given


from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.minifi.processor import Processor
from kafka_server_container import KafkaServer


@step("a {processor_name} processor set up to communicate with an Azure blob storage")
def step_impl(context: MinifiTestContext, processor_name: str):
    processor = Processor(processor_name, processor_name)
    hostname = f"http://azure-storage-server-{context.scenario_id}"
    processor.add_property('Container Name', 'test-container')
    processor.add_property('Connection String', 'DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint={hostname}:10000/devstoreaccount1;QueueEndpoint={hostname}:10001/devstoreaccount1;'.format(hostname=hostname))
    processor.add_property('Blob', 'test-blob')
    processor.add_property('Create Container', 'true')
    context.minifi_container.flow_definition.add_processor(processor)


@step("a Kafka server is set up")
def step_impl(context):
    context.containers.append(KafkaServer(context))


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
    context.minifi_container.flow_definition.add_processor(consume_kafka)


@step("the Kafka server is started")
def step_impl(context: MinifiTestContext):
    kafka_server_container = context.containers[0]
    assert isinstance(kafka_server_container, KafkaServer)
    assert kafka_server_container.deploy()


@step('the topic "{topic_name}" is initialized on the kafka broker')
def step_impl(context: MinifiTestContext, topic_name: str):
    kafka_server_container = context.containers[0]
    assert isinstance(kafka_server_container, KafkaServer)
    kafka_server_container.create_topic(topic_name=topic_name)


@when('a message with content "{message}" is published to the "{topic_name}" topic')
def step_impl(context: MinifiTestContext, message: str, topic_name: str):
    kafka_server_container = context.containers[0]
    assert isinstance(kafka_server_container, KafkaServer)
    kafka_server_container.produce_message(topic_name=topic_name, message=message)


@step('a message with content "{message}" is published to the "{topic_name}" topic with key "{key}"')
def step_impl(context: MinifiTestContext, message: str, topic_name: str, key: str):
    kafka_server_container = context.containers[0]
    assert isinstance(kafka_server_container, KafkaServer)
    kafka_server_container.produce_message_with_key(topic_name=topic_name, message=message, message_key=key)


@given("the \"{property_name}\" property of the {processor_name} processor is set to match {key_attribute_encoding} encoded kafka message key \"{message_key}\"")
def set_property_to_match_message_key(context, property_name, processor_name, key_attribute_encoding, message_key):
    if key_attribute_encoding.lower() == "hex":
        encoded_key = binascii.hexlify(message_key.encode("utf-8")).upper()
    elif key_attribute_encoding.lower() == "(not set)":
        encoded_key = message_key.encode("utf-8")
    else:
        encoded_key = message_key.encode(key_attribute_encoding)
    filtering = "${kafka.key:equals('" + encoded_key.decode("utf-8") + "')}"
    processor = context.minifi_container.flow_definition.get_processor(processor_name)
    processor.add_property(property_name, filtering)
