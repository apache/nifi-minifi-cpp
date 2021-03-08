from MiNiFi_integration_test_driver import MiNiFi_integration_test

from minifi.core.DockerTestCluster import DockerTestCluster
from minifi.core.FileSystemObserver import FileSystemObserver
from minifi.core.RemoteProcessGroup import RemoteProcessGroup
from minifi.core.InputPort import InputPort
from minifi.core.SSLContextService import SSLContextService
from minifi.core.SSL_cert_utils import gen_cert, gen_req, rsa_gen_key_callback

from minifi.processors.PublishKafka import PublishKafka
from minifi.processors.PutS3Object import PutS3Object
from minifi.processors.DeleteS3Object import DeleteS3Object
from minifi.processors.FetchS3Object import FetchS3Object
from minifi.processors.ListS3 import ListS3


from behave import given, then, when
from behave.model_describe import ModelDescriptor
from copy import copy
from copy import deepcopy
from pydoc import locate
from pytimeparse.timeparse import timeparse

import os
import logging
import re
import time
import uuid

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
    if property:
        processor.set_property(property, property_value)
    processor.set_name(processor_type)
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
    # PublishKafka is never the first node of a flow potential cluster-flow setup is omitted
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

@given("a PublishKafka processor set up to communicate with a kafka broker instance")
def step_impl(context):
    # PublishKafka is never the first node of a flow potential cluster-flow setup is omitted
    publish_kafka = PublishKafka()
    publish_kafka.set_name("PublishKafka")
    context.test.add_node(publish_kafka)

@given("the \"{property_name}\" of the {processor_name} processor is set to \"{property_value}\"")
def step_impl(context, property_name, processor_name, property_value):
    processor = context.test.get_node_by_name(processor_name)
    processor.set_property(property_name, property_value)


@given("the scheduling period of the {processor_name} processor is set to \"{sceduling_period}\"")
def step_impl(context, processor_name, sceduling_period):
    processor = context.test.get_node_by_name(processor_name)
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
#
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

@when("the MiNiFi instance starts up")
@when("both instances start up")
@when("all instances start up")
def step_impl(context):
    context.test.start()

@when("content \"{content}\" is added to file \"{file_name}\" present in directory \"{path}\" {seconds:d} seconds later")
def step_impl(context, content, file_name, path, seconds):
    time.sleep(seconds)
    context.test.add_test_data(path, content, file_name)

@then("a flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_file_with_content_generated(content, timeparse(duration))

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
