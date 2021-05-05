from minifi.core.FileSystemObserver import FileSystemObserver
from minifi.core.RemoteProcessGroup import RemoteProcessGroup
from minifi.core.SSLContextService import SSLContextService
from minifi.core.SSL_cert_utils import gen_cert, rsa_gen_key_callback

from minifi.processors.ConsumeKafka import ConsumeKafka
from minifi.processors.DeleteS3Object import DeleteS3Object
from minifi.processors.FetchS3Object import FetchS3Object
from minifi.processors.PutAzureBlobStorage import PutAzureBlobStorage
from minifi.processors.PublishKafka import PublishKafka
from minifi.processors.PutS3Object import PutS3Object

from behave import given, then, when
from behave.model_describe import ModelDescriptor
from pydoc import locate
from pytimeparse.timeparse import timeparse

import logging
import time
import uuid
import binascii

import docker

from kafka import KafkaProducer
from confluent_kafka.admin import AdminClient, NewTopic, NewPartitions
from confluent_kafka import Producer
import socket

# Background
@given("the content of \"{directory}\" is monitored")
def step_impl(context, directory):
    context.test.add_file_system_observer(FileSystemObserver(context.test.docker_path_to_local_path(directory)))

# MiNiFi cluster setups
@given("a {processor_type} processor with the \"{property}\" property set to \"{property_value}\" in a \"{cluster_name}\" flow")
@given("a {processor_type} processor with the \"{property}\" property set to \"{property_value}\" in the \"{cluster_name}\" flow")
def step_impl(context, processor_type, property, property_value, cluster_name):
    logging.info("Acquiring " + cluster_name)
    cluster = context.test.acquire_cluster(cluster_name)
    processor = locate("minifi.processors." + processor_type + "." + processor_type)()
    processor.set_name(processor_type)
    if property:
        processor.set_property(property, property_value)
    context.test.add_node(processor)
    # Assume that the first node declared is primary unless specified otherwise
    if cluster.get_flow() is None:
        cluster.set_name(cluster_name)
        cluster.set_flow(processor)


@given("a {processor_type} processor with the \"{property}\" property set to \"{property_value}\"")
def step_impl(context, processor_type, property, property_value):
    context.execute_steps("given a {processor_type} processor with the \"{property}\" property set to \"{property_value}\" in the \"{cluster_name}\" flow".
                          format(processor_type=processor_type, property=property, property_value=property_value, cluster_name="primary_cluster"))


@given("a {processor_type} processor in the \"{cluster_name}\" flow")
@given("a {processor_type} processor in a \"{cluster_name}\" flow")
def step_impl(context, processor_type, cluster_name):
    context.execute_steps("given a {processor_type} processor with the \"{property}\" property set to \"{property_value}\" in the \"{cluster_name}\" flow".
                          format(processor_type=processor_type, property=None, property_value=None, cluster_name=cluster_name))


@given("a {processor_type} processor")
def step_impl(context, processor_type):
    context.execute_steps("given a {processor_type} processor in the \"{cluster_name}\" flow".
        format(processor_type=processor_type, cluster_name="primary_cluster"))

@given("a set of processors in the \"{cluster_name}\" flow")
def step_impl(context, cluster_name):
    cluster = context.test.acquire_cluster(cluster_name)
    logging.info(context.table)
    for row in context.table:
        processor = locate("minifi.processors." + row["type"] + "." + row["type"])()
        processor.set_name(row["name"])
        processor.set_uuid(row["uuid"])
        context.test.add_node(processor)
        # Assume that the first node declared is primary unless specified otherwise
        if cluster.get_flow() is None:
            cluster.set_flow(processor)


@given("a set of processors")
def step_impl(context):
    rendered_table = ModelDescriptor.describe_table(context.table, "    ")
    context.execute_steps("""given a set of processors in the \"{cluster_name}\" flow
        {table}
        """.format(cluster_name="primary_cluster", table=rendered_table))


@given("a RemoteProcessGroup node opened on \"{address}\"")
def step_impl(context, address):
    remote_process_group = RemoteProcessGroup(address, "RemoteProcessGroup")
    context.test.add_remote_process_group(remote_process_group)


@given("a PutS3Object processor set up to communicate with an s3 server")
def step_impl(context):
    put_s3 = PutS3Object()
    put_s3.set_name("PutS3Object")
    context.test.add_node(put_s3)


@given("a DeleteS3Object processor set up to communicate with the same s3 server")
@given("a DeleteS3Object processor set up to communicate with an s3 server")
def step_impl(context):
    delete_s3 = DeleteS3Object()
    delete_s3.set_name("DeleteS3Object")
    context.test.add_node(delete_s3)


@given("a FetchS3Object processor set up to communicate with the same s3 server")
@given("a FetchS3Object processor set up to communicate with an s3 server")
def step_impl(context):
    fetch_s3 = FetchS3Object()
    fetch_s3.set_name("FetchS3Object")
    context.test.add_node(fetch_s3)


@given("a PutAzureBlobStorage processor set up to communicate with an Azure blob storage")
def step_impl(context):
    put_azure_blob_storage = PutAzureBlobStorage()
    put_azure_blob_storage.set_name("PutAzureBlobStorage")
    context.test.add_node(put_azure_blob_storage)


@given("a PublishKafka processor set up to communicate with a kafka broker instance")
def step_impl(context):
    # PublishKafka is never the first node of a flow potential cluster-flow setup is omitted
    publish_kafka = PublishKafka()
    publish_kafka.set_name("PublishKafka")
    context.test.add_node(publish_kafka)

@given("a kafka producer workflow publishing files placed in \"{directory}\" to a broker exactly once")
def step_impl(context, directory):
    context.execute_steps("""
        given a GetFile processor with the \"Input Directory\" property set to \"{directory}\"
        and the \"Keep Source File\" of the GetFile processor is set to \"false\"
        and a PublishKafka processor set up to communicate with a kafka broker instance
        and the "success" relationship of the GetFile processor is connected to the PublishKafka""".
        format(directory=directory))

@given("a ConsumeKafka processor set up in a \"{cluster_name}\" flow")
def step_impl(context, cluster_name):
    consume_kafka = ConsumeKafka()
    consume_kafka.set_name("ConsumeKafka")
    context.test.add_node(consume_kafka)
    logging.info("Acquiring " + cluster_name)
    cluster = context.test.acquire_cluster(cluster_name)
    # Assume that the first node declared is primary unless specified otherwise
    if cluster.get_flow() is None:
        cluster.set_name(cluster_name)
        cluster.set_flow(consume_kafka)

@given("the \"{property_name}\" of the {processor_name} processor is set to \"{property_value}\"")
def step_impl(context, property_name, processor_name, property_value):
    processor = context.test.get_node_by_name(processor_name)
    if property_value == "(not set)":
        processor.unset_property(property_name)
    else:
        processor.set_property(property_name, property_value)

@given("the \"{property_name}\" of the {processor_name} processor is set to match {key_attribute_encoding} encoded kafka message key \"{message_key}\"")
def step_impl(context, property_name, processor_name, key_attribute_encoding, message_key):
    encoded_key = ""
    if(key_attribute_encoding.lower() == "hex"):
        # Hex is presented upper-case to be in sync with NiFi
        encoded_key = binascii.hexlify(message_key.encode("utf-8")).upper()
    elif(key_attribute_encoding.lower() == "(not set)"):
        encoded_key = message_key.encode("utf-8")
    else:
        encoded_key = message_key.encode(key_attribute_encoding)
    logging.info("%s processor is set up to match encoded key \"%s\"", processor_name, encoded_key)
    filtering = "${kafka.key:equals('" + encoded_key.decode("utf-8") + "')}"
    logging.info("Filter: \"%s\"", filtering)
    processor = context.test.get_node_by_name(processor_name)
    processor.set_property(property_name, filtering)

@given("the \"{property_name}\" of the {processor_name} processor is set to match the attribute \"{attribute_key}\" to \"{attribute_value}\"")
def step_impl(context, property_name, processor_name, attribute_key, attribute_value):
    processor = context.test.get_node_by_name(processor_name)
    if attribute_value == "(not set)":
        # Ignore filtering
        processor.set_property(property_name, "true")
        return
    filtering = "${" +  attribute_key + ":equals('" + attribute_value + "')}"
    logging.info("Filter: \"%s\"", filtering)
    logging.info("Key: \"%s\", value: \"%s\"", attribute_key, attribute_value)
    processor.set_property(property_name, filtering)

@given("the scheduling period of the {processor_name} processor is set to \"{sceduling_period}\"")
def step_impl(context, processor_name, sceduling_period):
    processor = context.test.get_node_by_name(processor_name)
    processor.set_scheduling_strategy("TIMER_DRIVEN")
    processor.set_scheduling_period(sceduling_period)

@given("these processor properties are set")
@given("these processor properties are set to match the http proxy")
def step_impl(context):
    for row in context.table:
        context.test.get_node_by_name(row["processor name"]).set_property(row["property name"], row["property value"])

@given("the \"{relationship}\" relationship of the {source_name} processor is connected to the input port on the {remote_process_group_name}")
def step_impl(context, relationship, source_name, remote_process_group_name):
    source = context.test.get_node_by_name(source_name)
    remote_process_group = context.test.get_remote_process_group_by_name(remote_process_group_name)
    input_port_node = context.test.generate_input_port_for_remote_process_group(remote_process_group, "to_nifi")
    context.test.add_node(input_port_node)
    source.out_proc.connect({relationship: input_port_node})

@given("the \"{relationship}\" relationship of the {source_name} is connected to the {destination_name}")
@given("the \"{relationship}\" relationship of the {source_name} processor is connected to the {destination_name}")
def step_impl(context, relationship, source_name, destination_name):
    source = context.test.get_node_by_name(source_name)
    destination = context.test.get_node_by_name(destination_name)
    source.out_proc.connect({relationship: destination})

@given("the processors are connected up as described here")
def step_impl(context):
    for row in context.table:
        context.execute_steps(
            "given the \"" + row["relationship name"] + "\" relationship of the " + row["source name"] + " processor is connected to the " + row["destination name"])


@given("the connection going to the RemoteProcessGroup has \"drop empty\" set")
def step_impl(context):
    input_port = context.test.get_node_by_name("to_nifi")
    input_port.drop_empty_flowfiles = True


@given("a file with the content \"{content}\" is present in \"{path}\"")
def step_impl(context, content, path):
    context.test.add_test_data(path, content)


@given("a file with filename \"{file_name}\" and content \"{content}\" is present in \"{path}\"")
def step_impl(context, file_name, content, path):
    context.test.add_test_data(path, content, file_name)

@given("two files with content \"{content_1}\" and \"{content_2}\" are placed in \"{path}\"")
def step_impl(context, content_1, content_2, path):
    context.execute_steps("""
        given a file with the content \"{content_1}\" is present in \"{path}\"
        and a file with the content \"{content_2}\" is present in \"{path}\"""".
        format(content_1=content_1, content_2=content_2, path=path))

# NiFi setups
@given("a NiFi flow \"{cluster_name}\" receiving data from a RemoteProcessGroup \"{source_name}\" on port {port}")
def step_impl(context, cluster_name, source_name, port):
    remote_process_group = context.test.get_remote_process_group_by_name("RemoteProcessGroup")
    source = context.test.generate_input_port_for_remote_process_group(remote_process_group, "from-minifi")
    context.test.add_node(source)
    cluster = context.test.acquire_cluster(cluster_name)
    cluster.set_name('nifi')
    cluster.set_engine('nifi')
    # Assume that the first node declared is primary unless specified otherwise
    if cluster.get_flow() is None:
        cluster.set_flow(source)


@given("in the \"{cluster_name}\" flow the \"{relationship}\" relationship of the {source_name} processor is connected to the {destination_name}")
def step_impl(context, cluster_name, relationship, source_name, destination_name):
    cluster = context.test.acquire_cluster(cluster_name)
    source = context.test.get_or_create_node_by_name(source_name)
    destination = context.test.get_or_create_node_by_name(destination_name)
    source.out_proc.connect({relationship: destination})
    if cluster.get_flow() is None:
        cluster.set_flow(source)


# HTTP proxy setup
@given("the http proxy server \"{cluster_name}\" is set up")
@given("a http proxy server \"{cluster_name}\" is set up accordingly")
def step_impl(context, cluster_name):
    cluster = context.test.acquire_cluster(cluster_name)
    cluster.set_name(cluster_name)
    cluster.set_engine("http-proxy")
    cluster.set_flow(None)


# TLS
@given("an ssl context service set up for {producer_name} and {consumer_name}")
def step_impl(context, producer_name, consumer_name):
    cert, key = gen_cert()
    crt_file = '/tmp/resources/test-crt.pem'
    ssl_context_service = SSLContextService(cert=crt_file, ca_cert=crt_file)
    context.test.put_test_resource('test-crt.pem', cert.as_pem() + key.as_pem(None, rsa_gen_key_callback))
    producer = context.test.get_node_by_name(producer_name)
    producer.controller_services.append(ssl_context_service)
    producer.set_property("SSL Context Service", ssl_context_service.name)
    consumer = context.test.get_node_by_name(consumer_name)
    consumer.set_property("SSL Certificate", crt_file)
    consumer.set_property("SSL Verify Peer", "no")

# Kafka setup
@given("a kafka broker \"{cluster_name}\" is set up in correspondence with the PublishKafka")
@given("a kafka broker \"{cluster_name}\" is set up in correspondence with the third-party kafka publisher")
@given("a kafka broker \"{cluster_name}\" is set up in correspondence with the publisher flow")
def step_impl(context, cluster_name):
    cluster = context.test.acquire_cluster(cluster_name)
    cluster.set_name(cluster_name)
    cluster.set_engine("kafka-broker")
    cluster.set_flow(None)


# s3 setup
@given("a s3 server \"{cluster_name}\" is set up in correspondence with the PutS3Object")
@given("a s3 server \"{cluster_name}\" is set up in correspondence with the DeleteS3Object")
def step_impl(context, cluster_name):
    cluster = context.test.acquire_cluster(cluster_name)
    cluster.set_name(cluster_name)
    cluster.set_engine("s3-server")
    cluster.set_flow(None)


# azure storage setup
@given("an Azure storage server \"{cluster_name}\" is set up in correspondence with the PutAzureBlobStorage")
def step_impl(context, cluster_name):
    cluster = context.test.acquire_cluster(cluster_name)
    cluster.set_name(cluster_name)
    cluster.set_engine("azure-storage-server")
    cluster.set_flow(None)

@given("the kafka broker \"{cluster_name}\" is started")
def step_impl(context, cluster_name):
    context.test.start_single_cluster(cluster_name)

@given("the topic \"{topic_name}\" is initialized on the kafka broker")
def step_impl(context, topic_name):
    admin = AdminClient({'bootstrap.servers': "localhost:29092"})
    new_topics = [NewTopic(topic_name, num_partitions=3, replication_factor=1)]
    futures = admin.create_topics(new_topics)
    # Block until the topic is created
    for topic, future in futures.items():
        try:
            future.result()
            print("Topic {} created".format(topic))
        except Exception as e:
            print("Failed to create topic {}: {}".format(topic, e))
 
@when("the MiNiFi instance starts up")
@when("both instances start up")
@when("all instances start up")
@when("all other processes start up")
def step_impl(context):
    context.test.start()


@when("content \"{content}\" is added to file \"{file_name}\" present in directory \"{path}\" {seconds:d} seconds later")
def step_impl(context, content, file_name, path, seconds):
    time.sleep(seconds)
    context.test.add_test_data(path, content, file_name)

# Kafka
def delivery_report(err, msg):
    if err is not None:
        logging.info('Message delivery failed: {}'.format(err))
    else:
        logging.info('Message delivered to {} [{}]'.format(msg.topic(), msg.partition()))

@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic")
def step_impl(context, content, topic_name):
    producer = Producer({"bootstrap.servers": "localhost:29092", "client.id": socket.gethostname()})
    producer.produce(topic_name, content.encode("utf-8"), callback=delivery_report)
    producer.flush(10)

# Used for testing transactional message consumption
@when("the publisher performs a {transaction_type} transaction publishing to the \"{topic_name}\" topic these messages: {messages}")
def step_impl(context, transaction_type, topic_name, messages):
    producer = Producer({"bootstrap.servers": "localhost:29092", "transactional.id": "1001"})
    producer.init_transactions()
    logging.info("Transaction type: %s", transaction_type)
    logging.info("Messages: %s", messages.split(", "))
    if transaction_type == "SINGLE_COMMITTED_TRANSACTION":
        producer.begin_transaction()
        for content in messages.split(", "):
            producer.produce(topic_name, content.encode("utf-8"), callback=delivery_report)
        producer.commit_transaction()
        producer.flush(10)
    elif transaction_type == "TWO_SEPARATE_TRANSACTIONS":
        for content in messages.split(", "):
            producer.begin_transaction()
            producer.produce(topic_name, content.encode("utf-8"), callback=delivery_report)
            producer.commit_transaction()
        producer.flush(10)
    elif transaction_type == "NON_COMMITTED_TRANSACTION":
        producer.begin_transaction()
        for content in messages.split(", "):
            producer.produce(topic_name, content.encode("utf-8"), callback=delivery_report)
        producer.flush(10)
    elif transaction_type == "CANCELLED_TRANSACTION":
        producer.begin_transaction()
        for content in messages.split(", "):
            producer.produce(topic_name, content.encode("utf-8"), callback=delivery_report)
        producer.flush(10)
        producer.abort_transaction()
    else:
        raise Exception("Unknown transaction type.")

@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic with key \"{message_key}\"")
def step_impl(context, content, topic_name, message_key):
    producer = Producer({"bootstrap.servers": "localhost:29092", "client.id": socket.gethostname()})
    # Asynchronously produce a message, the delivery report callback
    # will be triggered from poll() above, or flush() below, when the message has
    # been successfully delivered or failed permanently.
    producer.produce(topic_name, content.encode("utf-8"), callback=delivery_report, key=message_key.encode("utf-8"))
    # Wait for any outstanding messages to be delivered and delivery report
    # callbacks to be triggered.
    producer.flush(10)

@when("{number_of_messages} kafka messages are sent to the topic \"{topic_name}\"")
def step_impl(context, number_of_messages, topic_name):
    producer = Producer({"bootstrap.servers": "localhost:29092", "client.id": socket.gethostname()})
    for i in range(0, int(number_of_messages)):
        producer.produce(topic_name, str(uuid.uuid4()).encode("utf-8"))
    producer.flush(10)

@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic with headers \"{semicolon_separated_headers}\"")
def step_impl(context, content, topic_name, semicolon_separated_headers):
    # Confluent kafka does not support multiple headers with same key, another API must be used here.
    headers = []
    for header in semicolon_separated_headers.split(";"):
        kv = header.split(":")
        headers.append((kv[0].strip(), kv[1].strip().encode("utf-8")))
    producer = KafkaProducer(bootstrap_servers='localhost:29092')
    future = producer.send(topic_name, content.encode("utf-8"), headers=headers)
    result = future.get(timeout=60)

@then("a flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_single_file_with_content_generated(content, timeparse(duration))

@then("at least one flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_at_least_one_file_with_content_generated(content, timeparse(duration))

@then("{num_flowfiles} flowfiles are placed in the monitored directory in less than {duration}")
def step_impl(context, num_flowfiles, duration):
    if num_flowfiles == 0:
        context.execute_steps("""no files are placed in the monitored directory in {duration} of running time""".
            format(duration=duration))
        return
    context.test.check_for_num_files_generated(int(num_flowfiles), timeparse(duration))

@then("two flowfiles with the contents \"{content_1}\" and \"{content_2}\" are placed in the monitored directory in less than {duration}")
def step_impl(context, content_1, content_2, duration):
    wait_start_time = time.perf_counter()
    context.test.wait_for_multiple_output_files(timeparse(duration), 2)
    context.execute_steps("""
        then a flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}""".
        format(content=content_1, duration=duration))
    context.execute_steps("""
        then a flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}""".
        format(content=content_2, duration=str(timeparse(duration) - (time.perf_counter() - wait_start_time)) + " seconds"))

@then("flowfiles with these contents are placed in the monitored directory in less than {duration}: \"{contents}\"")
def step_impl(context, duration, contents):
    # This works but is slow
    for content in contents.split(","):
        context.test.check_for_at_least_one_file_with_content_generated(content, timeparse(duration))

@then("minimum {lower_bound}, maximum {upper_bound} flowfiles are produced and placed in the monitored directory in less than {duration}")
def step_impl(context, lower_bound, upper_bound, duration):
    context.test.check_for_num_file_range_generated(int(lower_bound), int(upper_bound), timeparse(duration))

@then("{number_of_files:d} flowfiles are placed in the monitored directory in {duration}")
@then("{number_of_files:d} flowfile is placed in the monitored directory in {duration}")
def step_impl(context, number_of_files, duration):
    context.test.check_for_multiple_files_generated(number_of_files, timeparse(duration))


@then("at least one empty flowfile is placed in the monitored directory in less than {duration}")
def step_impl(context, duration):
    context.test.check_for_multiple_empty_files_generated(timeparse(duration))


@then("no files are placed in the monitored directory in {duration} of running time")
def step_impl(context, duration):
    context.test.check_for_no_files_generated(timeparse(duration))


@then("no errors were generated on the \"{cluster_name}\" regarding \"{url}\"")
def step_impl(context, cluster_name, url):
    context.test.check_http_proxy_access(cluster_name, url)


@then("the object on the \"{cluster_name}\" s3 server is \"{object_data}\"")
def step_impl(context, cluster_name, object_data):
    context.test.check_s3_server_object_data(cluster_name, object_data)


@then("the object content type on the \"{cluster_name}\" s3 server is \"{content_type}\" and the object metadata matches use metadata")
def step_impl(context, cluster_name, content_type):
    context.test.check_s3_server_object_metadata(cluster_name, content_type)


@then("the object bucket on the \"{cluster_name}\" s3 server is empty")
def step_impl(context, cluster_name):
    context.test.check_empty_s3_bucket(cluster_name)


@then("the object on the \"{cluster_name}\" Azure storage server is \"{object_data}\"")
def step_impl(context, cluster_name, object_data):
    context.test.check_azure_storage_server_data(cluster_name, object_data)
