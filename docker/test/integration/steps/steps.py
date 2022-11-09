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


from minifi.core.FileSystemObserver import FileSystemObserver
from minifi.core.RemoteProcessGroup import RemoteProcessGroup
from minifi.core.SSL_cert_utils import make_ca, make_cert, dump_certificate, dump_privatekey
from minifi.core.Funnel import Funnel

from minifi.controllers.SSLContextService import SSLContextService
from minifi.controllers.GCPCredentialsControllerService import GCPCredentialsControllerService
from minifi.controllers.ElasticsearchCredentialsService import ElasticsearchCredentialsService
from minifi.controllers.ODBCService import ODBCService
from minifi.controllers.KubernetesControllerService import KubernetesControllerService

from behave import given, then, when
from behave.model_describe import ModelDescriptor
from pydoc import locate
from pytimeparse.timeparse import timeparse

import logging
import time
import uuid
import binascii

from kafka import KafkaProducer
from confluent_kafka.admin import AdminClient, NewTopic
from confluent_kafka import Producer
import socket
import os


# Background
@given("the content of \"{directory}\" is monitored")
def step_impl(context, directory):
    context.test.add_file_system_observer(FileSystemObserver(context.directory_bindings.docker_path_to_local_path(context.test_id, directory)))


def __create_processor(context, processor_type, processor_name, property_name, property_value, container_name, engine='minifi-cpp'):
    container = context.test.acquire_container(container_name, engine)
    processor = locate("minifi.processors." + processor_type + "." + processor_type)()
    processor.set_name(processor_name)
    if property_name is not None:
        processor.set_property(property_name, property_value)
    context.test.add_node(processor)
    # Assume that the first node declared is primary unless specified otherwise
    if not container.get_start_nodes():
        container.add_start_node(processor)


# MiNiFi cluster setups
@given("a {processor_type} processor with the name \"{processor_name}\" and the \"{property_name}\" property set to \"{property_value}\" in a \"{minifi_container_name}\" flow")
@given("a {processor_type} processor with the name \"{processor_name}\" and the \"{property_name}\" property set to \"{property_value}\" in the \"{minifi_container_name}\" flow")
def step_impl(context, processor_type, processor_name, property_name, property_value, minifi_container_name):
    __create_processor(context, processor_type, processor_name, property_name, property_value, minifi_container_name)


@given("a {processor_type} processor with the name \"{processor_name}\" and the \"{property_name}\" property set to \"{property_value}\" in a \"{minifi_container_name}\" flow with engine \"{engine_name}\"")
@given("a {processor_type} processor with the name \"{processor_name}\" and the \"{property_name}\" property set to \"{property_value}\" in the \"{minifi_container_name}\" flow with engine \"{engine_name}\"")
def step_impl(context, processor_type, processor_name, property_name, property_value, minifi_container_name, engine_name):
    __create_processor(context, processor_type, processor_name, property_name, property_value, minifi_container_name, engine_name)


@given("a {processor_type} processor with the \"{property_name}\" property set to \"{property_value}\" in a \"{minifi_container_name}\" flow")
@given("a {processor_type} processor with the \"{property_name}\" property set to \"{property_value}\" in the \"{minifi_container_name}\" flow")
def step_impl(context, processor_type, property_name, property_value, minifi_container_name):
    __create_processor(context, processor_type, processor_type, property_name, property_value, minifi_container_name)


@given("a {processor_type} processor the \"{property_name}\" property set to \"{property_value}\" in the \"{minifi_container_name}\" flow with engine \"{engine_name}\"")
def step_impl(context, processor_type, property_name, property_value, minifi_container_name, engine_name):
    __create_processor(context, processor_type, processor_type, property_name, property_value, minifi_container_name, engine_name)


@given("a {processor_type} processor with the \"{property_name}\" property set to \"{property_value}\"")
def step_impl(context, processor_type, property_name, property_value):
    __create_processor(context, processor_type, processor_type, property_name, property_value, "minifi-cpp-flow")


@given("a {processor_type} processor with the name \"{processor_name}\" and the \"{property_name}\" property set to \"{property_value}\"")
def step_impl(context, processor_type, property_name, property_value, processor_name):
    __create_processor(context, processor_type, processor_name, property_name, property_value, "minifi-cpp-flow")


@given("a {processor_type} processor with the name \"{processor_name}\" in the \"{minifi_container_name}\" flow")
def step_impl(context, processor_type, processor_name, minifi_container_name):
    __create_processor(context, processor_type, processor_name, None, None, minifi_container_name)


@given("a {processor_type} processor with the name \"{processor_name}\" in the \"{minifi_container_name}\" flow with engine \"{engine_name}\"")
def step_impl(context, processor_type, processor_name, minifi_container_name, engine_name):
    __create_processor(context, processor_type, processor_name, None, None, minifi_container_name, engine_name)


@given("a {processor_type} processor with the name \"{processor_name}\"")
def step_impl(context, processor_type, processor_name):
    __create_processor(context, processor_type, processor_name, None, None, "minifi-cpp-flow")


@given("a {processor_type} processor in the \"{minifi_container_name}\" flow")
@given("a {processor_type} processor in a \"{minifi_container_name}\" flow")
@given("a {processor_type} processor set up in a \"{minifi_container_name}\" flow")
def step_impl(context, processor_type, minifi_container_name):
    __create_processor(context, processor_type, processor_type, None, None, minifi_container_name)


@given("a {processor_type} processor")
@given("a {processor_type} processor set up to communicate with an s3 server")
@given("a {processor_type} processor set up to communicate with the same s3 server")
@given("a {processor_type} processor set up to communicate with an Azure blob storage")
@given("a {processor_type} processor set up to communicate with a kafka broker instance")
@given("a {processor_type} processor set up to communicate with an MQTT broker instance")
@given("a {processor_type} processor set up to communicate with the Splunk HEC instance")
def step_impl(context, processor_type):
    __create_processor(context, processor_type, processor_type, None, None, "minifi-cpp-flow")


@given("a {processor_type} processor in a Kubernetes cluster")
@given("a {processor_type} processor in the Kubernetes cluster")
def step_impl(context, processor_type):
    __create_processor(context, processor_type, processor_type, None, None, "kubernetes", "kubernetes")


@given("a set of processors in the \"{minifi_container_name}\" flow")
def step_impl(context, minifi_container_name):
    container = context.test.acquire_container(minifi_container_name)
    logging.info(context.table)
    for row in context.table:
        processor = locate("minifi.processors." + row["type"] + "." + row["type"])()
        processor.set_name(row["name"])
        processor.set_uuid(row["uuid"])
        context.test.add_node(processor)
        # Assume that the first node declared is primary unless specified otherwise
        if not container.get_start_nodes():
            container.add_start_node(processor)


@given("a set of processors")
def step_impl(context):
    rendered_table = ModelDescriptor.describe_table(context.table, "    ")
    context.execute_steps("""given a set of processors in the \"{minifi_container_name}\" flow
        {table}
        """.format(minifi_container_name="minifi-cpp-flow", table=rendered_table))


@given("a RemoteProcessGroup node opened on \"{address}\"")
def step_impl(context, address):
    remote_process_group = RemoteProcessGroup(address, "RemoteProcessGroup")
    context.test.add_remote_process_group(remote_process_group)


@given("a kafka producer workflow publishing files placed in \"{directory}\" to a broker exactly once")
def step_impl(context, directory):
    context.execute_steps("""
        given a GetFile processor with the \"Input Directory\" property set to \"{directory}\"
        and the \"Keep Source File\" property of the GetFile processor is set to \"false\"
        and a PublishKafka processor set up to communicate with a kafka broker instance
        and the "success" relationship of the GetFile processor is connected to the PublishKafka""".format(directory=directory))


@given("the \"{property_name}\" property of the {processor_name} processor is set to \"{property_value}\"")
def step_impl(context, property_name, processor_name, property_value):
    processor = context.test.get_node_by_name(processor_name)
    if property_value == "(not set)":
        processor.unset_property(property_name)
    else:
        processor.set_property(property_name, property_value)


@given("the \"{property_name}\" properties of the {processor_name_one} and {processor_name_two} processors are set to the same random guid")
def step_impl(context, property_name, processor_name_one, processor_name_two):
    uuid_str = str(uuid.uuid4())
    context.test.get_node_by_name(processor_name_one).set_property(property_name, uuid_str)
    context.test.get_node_by_name(processor_name_two).set_property(property_name, uuid_str)


@given("the max concurrent tasks attribute of the {processor_name} processor is set to {max_concurrent_tasks:d}")
def step_impl(context, processor_name, max_concurrent_tasks):
    processor = context.test.get_node_by_name(processor_name)
    processor.set_max_concurrent_tasks(max_concurrent_tasks)


@given("the \"{property_name}\" property of the {processor_name} processor is set to match {key_attribute_encoding} encoded kafka message key \"{message_key}\"")
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


@given("the \"{property_name}\" property of the {processor_name} processor is set to match the attribute \"{attribute_key}\" to \"{attribute_value}\"")
def step_impl(context, property_name, processor_name, attribute_key, attribute_value):
    processor = context.test.get_node_by_name(processor_name)
    if attribute_value == "(not set)":
        # Ignore filtering
        processor.set_property(property_name, "true")
        return
    filtering = "${" + attribute_key + ":equals('" + attribute_value + "')}"
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
@then("a file with the content \"{content}\" is placed in \"{path}\"")
def step_impl(context, content, path):
    context.test.add_test_data(path, content)


@given("{number_of_files:d} files with the content \"{content}\" are present in \"{path}\"")
def step_impl(context, number_of_files, content, path):
    for i in range(0, number_of_files):
        context.test.add_test_data(path, content)


@given("an empty file is present in \"{path}\"")
def step_impl(context, path):
    context.test.add_test_data(path, "")


@given("a file with filename \"{file_name}\" and content \"{content}\" is present in \"{path}\"")
def step_impl(context, file_name, content, path):
    context.test.add_test_data(path, content, file_name)


@given("a Funnel with the name \"{funnel_name}\" is set up")
def step_impl(context, funnel_name):
    funnel = Funnel(funnel_name)
    context.test.add_node(funnel)


@given("the Funnel with the name \"{source_name}\" is connected to the {destination_name}")
def step_impl(context, source_name, destination_name):
    source = context.test.get_or_create_node_by_name(source_name)
    destination = context.test.get_or_create_node_by_name(destination_name)
    source.out_proc.connect({'success': destination})


@given("\"{processor_name}\" processor is a start node")
def step_impl(context, processor_name):
    container = context.test.acquire_container("minifi-cpp-flow")
    processor = context.test.get_or_create_node_by_name(processor_name)
    container.add_start_node(processor)


# NiFi setups
@given("a NiFi flow receiving data from a RemoteProcessGroup \"{source_name}\" on port 8080")
def step_impl(context, source_name):
    remote_process_group = context.test.get_remote_process_group_by_name("RemoteProcessGroup")
    source = context.test.generate_input_port_for_remote_process_group(remote_process_group, source_name)
    context.test.add_node(source)
    container = context.test.acquire_container('nifi', 'nifi')
    # Assume that the first node declared is primary unless specified otherwise
    if not container.get_start_nodes():
        container.add_start_node(source)


@given("a NiFi flow with the name \"{flow_name}\" is set up")
def step_impl(context, flow_name):
    context.test.acquire_container(flow_name, 'nifi')


# HTTP proxy setup
@given("the http proxy server is set up")
@given("a http proxy server is set up accordingly")
def step_impl(context):
    context.test.acquire_container("http-proxy", "http-proxy")


# TLS
@given("an ssl context service set up for {producer_name} and {consumer_name}")
def step_impl(context, producer_name, consumer_name):
    root_ca_cert, root_ca_key = make_ca("root CA")
    minifi_cert, minifi_key = make_cert("minifi-cpp-flow", root_ca_cert, root_ca_key)
    secondary_cert, secondary_key = make_cert("secondary", root_ca_cert, root_ca_key)
    secondary_pem_file = '/tmp/resources/secondary.pem'
    minifi_crt_file = '/tmp/resources/minifi-cpp-flow.crt'
    minifi_key_file = '/tmp/resources/minifi-cpp-flow.key'
    root_ca_crt_file = '/tmp/resources/root_ca.pem'
    ssl_context_service = SSLContextService(cert=minifi_crt_file, ca_cert=root_ca_crt_file, key=minifi_key_file)
    context.test.put_test_resource('secondary.pem', dump_certificate(secondary_cert) + dump_privatekey(secondary_key))
    context.test.put_test_resource('minifi-cpp-flow.crt', dump_certificate(minifi_cert))
    context.test.put_test_resource('minifi-cpp-flow.key', dump_privatekey(minifi_key))
    context.test.put_test_resource('root_ca.pem', dump_certificate(root_ca_cert))

    producer = context.test.get_node_by_name(producer_name)
    producer.controller_services.append(ssl_context_service)
    producer.set_property("SSL Context Service", ssl_context_service.name)
    consumer = context.test.get_node_by_name(consumer_name)
    consumer.set_property("SSL Certificate Authority", root_ca_crt_file)
    consumer.set_property("SSL Certificate", secondary_pem_file)
    consumer.set_property("SSL Verify Peer", "yes")


@given("an ssl context service set up for {producer_name}")
def step_impl(context, producer_name):
    ssl_context_service = SSLContextService(cert="/tmp/resources/certs/client_cert.pem", ca_cert="/tmp/resources/certs/ca-cert", key="/tmp/resources/certs/client_cert.key", passphrase="abcdefgh")
    producer = context.test.get_node_by_name(producer_name)
    producer.controller_services.append(ssl_context_service)
    producer.set_property("SSL Context Service", ssl_context_service.name)


# Kubernetes
def __set_up_the_kubernetes_controller_service(context, processor_name, service_property_name, properties):
    kubernetes_controller_service = KubernetesControllerService("Kubernetes Controller Service", properties)
    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(kubernetes_controller_service)
    processor.set_property(service_property_name, kubernetes_controller_service.name)


@given("the {processor_name} processor has a {service_property_name} which is a Kubernetes Controller Service")
@given("the {processor_name} processor has an {service_property_name} which is a Kubernetes Controller Service")
def step_impl(context, processor_name, service_property_name):
    __set_up_the_kubernetes_controller_service(context, processor_name, service_property_name, {})


@given("the {processor_name} processor has a {service_property_name} which is a Kubernetes Controller Service with the \"{property_name}\" property set to \"{property_value}\"")
@given("the {processor_name} processor has an {service_property_name} which is a Kubernetes Controller Service with the \"{property_name}\" property set to \"{property_value}\"")
def step_impl(context, processor_name, service_property_name, property_name, property_value):
    __set_up_the_kubernetes_controller_service(context, processor_name, service_property_name, {property_name: property_value})


# Kafka setup
@given("a kafka broker is set up in correspondence with the PublishKafka")
@given("a kafka broker is set up in correspondence with the third-party kafka publisher")
@given("a kafka broker is set up in correspondence with the publisher flow")
def step_impl(context):
    context.test.acquire_container("kafka-broker", "kafka-broker")


# MQTT setup
@given("an MQTT broker is set up in correspondence with the PublishMQTT")
@given("an MQTT broker is set up in correspondence with the ConsumeMQTT")
@given("an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT")
def step_impl(context):
    context.test.acquire_container("mqtt-broker", "mqtt-broker")


# s3 setup
@given("a s3 server is set up in correspondence with the PutS3Object")
@given("a s3 server is set up in correspondence with the DeleteS3Object")
def step_impl(context):
    context.test.acquire_container("s3-server", "s3-server")


# azure storage setup
@given("an Azure storage server is set up")
def step_impl(context):
    context.test.acquire_container("azure-storage-server", "azure-storage-server")


# syslog client
@given(u'a Syslog client with {protocol} protocol is setup to send logs to minifi')
def step_impl(context, protocol):
    client_name = "syslog-" + protocol.lower() + "-client"
    context.test.acquire_container(client_name, client_name)


# google cloud storage setup
@given("a Google Cloud storage server is set up")
@given("a Google Cloud storage server is set up with some test data")
@given('a Google Cloud storage server is set up and a single object with contents "preloaded data" is present')
def step_impl(context):
    context.test.acquire_container("fake-gcs-server", "fake-gcs-server")


# elasticsearch
@given('an Elasticsearch server is set up and running')
@given('an Elasticsearch server is set up and a single document is present with "preloaded_id" in "my_index"')
@given('an Elasticsearch server is set up and a single document is present with "preloaded_id" in "my_index" with "value1" in "field1"')
def step_impl(context):
    context.test.start_elasticsearch()
    context.test.create_doc_elasticsearch("elasticsearch", "my_index", "preloaded_id")


# opensearch
@given('an Opensearch server is set up and running')
@given('an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index"')
@given('an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index" with "value1" in "field1"')
def step_impl(context):
    context.test.start_opensearch()
    context.test.add_elastic_user_to_opensearch("opensearch")
    context.test.create_doc_elasticsearch("opensearch", "my_index", "preloaded_id")


@given(u'a SSL context service is set up for PostElasticsearch and Elasticsearch')
def step_impl(context):
    minifi_crt_file = '/tmp/resources/elasticsearch/minifi_client.crt'
    minifi_key_file = '/tmp/resources/elasticsearch/minifi_client.key'
    root_ca_crt_file = '/tmp/resources/elasticsearch/root_ca.crt'
    ssl_context_service = SSLContextService(cert=minifi_crt_file, ca_cert=root_ca_crt_file, key=minifi_key_file)
    post_elasticsearch_json = context.test.get_node_by_name("PostElasticsearch")
    post_elasticsearch_json.controller_services.append(ssl_context_service)
    post_elasticsearch_json.set_property("SSL Context Service", ssl_context_service.name)


@given(u'a SSL context service is set up for PostElasticsearch and Opensearch')
def step_impl(context):
    root_ca_crt_file = '/tmp/resources/opensearch/root-ca.pem'
    ssl_context_service = SSLContextService(ca_cert=root_ca_crt_file)
    post_elasticsearch_json = context.test.get_node_by_name("PostElasticsearch")
    post_elasticsearch_json.controller_services.append(ssl_context_service)
    post_elasticsearch_json.set_property("SSL Context Service", ssl_context_service.name)


@given(u'an ElasticsearchCredentialsService is set up for PostElasticsearch with Basic Authentication')
def step_impl(context):
    elasticsearch_credential_service = ElasticsearchCredentialsService()
    post_elasticsearch_json = context.test.get_node_by_name("PostElasticsearch")
    post_elasticsearch_json.controller_services.append(elasticsearch_credential_service)
    post_elasticsearch_json.set_property("Elasticsearch Credentials Provider Service", elasticsearch_credential_service.name)


@given(u'an ElasticsearchCredentialsService is set up for PostElasticsearch with ApiKey')
def step_impl(context):
    api_key = context.test.elastic_generate_apikey("elasticsearch")
    elasticsearch_credential_service = ElasticsearchCredentialsService(api_key)
    post_elasticsearch_json = context.test.get_node_by_name("PostElasticsearch")
    post_elasticsearch_json.controller_services.append(elasticsearch_credential_service)
    post_elasticsearch_json.set_property("Elasticsearch Credentials Provider Service", elasticsearch_credential_service.name)


# splunk hec
@given("a Splunk HEC is set up and running")
def step_impl(context):
    context.test.start_splunk()


# TCP client
@given('a TCP client is set up to send a test TCP message to minifi')
def step_impl(context):
    context.test.acquire_container("tcp-client", "tcp-client")


@given("SSL is enabled for the Splunk HEC and the SSL context service is set up for PutSplunkHTTP and QuerySplunkIndexingStatus")
def step_impl(context):
    root_ca_cert, root_ca_key = make_ca("root CA")
    minifi_cert, minifi_key = make_cert("minifi-cpp-flow", root_ca_cert, root_ca_key)
    splunk_cert, splunk_key = make_cert("splunk", root_ca_cert, root_ca_key)
    minifi_crt_file = '/tmp/resources/minifi-cpp-flow.pem'
    minifi_key_file = '/tmp/resources/minifi-cpp-flow.key'
    root_ca_crt_file = '/tmp/resources/root_ca.pem'
    ssl_context_service = SSLContextService(cert=minifi_crt_file, ca_cert=root_ca_crt_file, key=minifi_key_file)
    context.test.put_test_resource('minifi-cpp-flow.pem', dump_certificate(minifi_cert))
    context.test.put_test_resource('minifi-cpp-flow.key', dump_privatekey(minifi_key))
    context.test.put_test_resource('root_ca.pem', dump_certificate(root_ca_cert))

    put_splunk_http = context.test.get_node_by_name("PutSplunkHTTP")
    put_splunk_http.controller_services.append(ssl_context_service)
    put_splunk_http.set_property("SSL Context Service", ssl_context_service.name)
    query_splunk_indexing_status = context.test.get_node_by_name("QuerySplunkIndexingStatus")
    query_splunk_indexing_status.controller_services.append(ssl_context_service)
    query_splunk_indexing_status.set_property("SSL Context Service", ssl_context_service.name)
    context.test.cluster.enable_splunk_hec_ssl('splunk', dump_certificate(splunk_cert), dump_privatekey(splunk_key), dump_certificate(root_ca_cert))


@given(u'the {processor_one} processor is set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server')
def step_impl(context, processor_one):
    gcp_controller_service = GCPCredentialsControllerService(credentials_location="Use Anonymous credentials")
    p1 = context.test.get_node_by_name(processor_one)
    p1.controller_services.append(gcp_controller_service)
    p1.set_property("GCP Credentials Provider Service", gcp_controller_service.name)


@given(u'the {processor_one} and the {processor_two} processors are set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server')
def step_impl(context, processor_one, processor_two):
    gcp_controller_service = GCPCredentialsControllerService(credentials_location="Use Anonymous credentials")
    p1 = context.test.get_node_by_name(processor_one)
    p2 = context.test.get_node_by_name(processor_two)
    p1.controller_services.append(gcp_controller_service)
    p1.set_property("GCP Credentials Provider Service", gcp_controller_service.name)
    p2.controller_services.append(gcp_controller_service)
    p2.set_property("GCP Credentials Provider Service", gcp_controller_service.name)


@given("the kafka broker is started")
def step_impl(context):
    context.test.start_kafka_broker()


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


# SQL
@given("an ODBCService is setup up for {processor_name} with the name \"{service_name}\"")
def step_impl(context, processor_name, service_name):
    odbc_service = ODBCService(name=service_name, connection_string="Driver={PostgreSQL ANSI};Server=postgresql-server;Port=5432;Database=postgres;Uid=postgres;Pwd=password;")
    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(odbc_service)
    processor.set_property("DB Controller Service", odbc_service.name)


@given("a PostgreSQL server is set up")
def step_impl(context):
    context.test.acquire_container("postgresql-server", "postgresql-server")


# OPC UA
@given("an OPC UA server is set up")
def step_impl(context):
    context.test.acquire_container("opcua-server", "opcua-server")


@given("an OPC UA server is set up with access control")
def step_impl(context):
    context.test.acquire_container("opcua-server", "opcua-server", ["/opt/open62541/examples/access_control_server"])


@when("the MiNiFi instance starts up")
@when("both instances start up")
@when("all instances start up")
@when("all other processes start up")
def step_impl(context):
    context.test.start()


@then("\"{container_name}\" flow is stopped")
def step_impl(context, container_name):
    context.test.stop(container_name)


@then("\"{container_name}\" flow is killed")
def step_impl(context, container_name):
    context.test.kill(container_name)


@then("\"{container_name}\" flow is restarted")
def step_impl(context, container_name):
    context.test.restart(container_name)


@when("\"{container_name}\" flow is started")
@then("\"{container_name}\" flow is started")
def step_impl(context, container_name):
    context.test.start(container_name)


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


@when("a message with content \"{content}\" is published to the \"{topic_name}\" topic using an ssl connection")
def step_impl(context, content, topic_name):
    test_dir = os.environ['TEST_DIRECTORY']  # Based on DockerVerify.sh
    producer = Producer({
        "bootstrap.servers": "localhost:29093",
        "security.protocol": "ssl",
        "ssl.ca.location": os.path.join(test_dir, "resources/kafka_broker/conf/certs/ca-cert"),
        "ssl.certificate.location": os.path.join(test_dir, "resources/kafka_broker/conf/certs/client_cert.pem"),
        "ssl.key.location": os.path.join(test_dir, "resources/kafka_broker/conf/certs/client_cert.key"),
        "ssl.key.password": "abcdefgh",
        "client.id": socket.gethostname()})
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
    assert future.get(timeout=60)


@when("the Kafka consumer is registered in kafka broker")
def step_impl(context):
    context.test.wait_for_kafka_consumer_to_be_registered("kafka-broker")
    # After the consumer is registered there is still some time needed for consumer-broker synchronization
    # Unfortunately there are no additional log messages that could be checked for this
    time.sleep(2)


@then("a flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}")
@then("a flowfile with the content '{content}' is placed in the monitored directory in less than {duration}")
@then("{number_of_flow_files:d} flowfiles with the content \"{content}\" are placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration, number_of_flow_files=1):
    context.test.check_for_multiple_files_generated(number_of_flow_files, timeparse(duration), [content])


@then("a flowfile with the JSON content \"{content}\" is placed in the monitored directory in less than {duration}")
@then("a flowfile with the JSON content '{content}' is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_single_json_file_with_content_generated(content, timeparse(duration))


@then("at least one flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}")
@then("at least one flowfile with the content '{content}' is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_at_least_one_file_with_content_generated(content, timeparse(duration))


@then("{num_flowfiles} flowfiles are placed in the monitored directory in less than {duration}")
def step_impl(context, num_flowfiles, duration):
    if num_flowfiles == 0:
        context.execute_steps("""no files are placed in the monitored directory in {duration} of running time""".format(duration=duration))
        return
    context.test.check_for_num_files_generated(int(num_flowfiles), timeparse(duration))


@then("at least one flowfile is placed in the monitored directory in less than {duration}")
def step_impl(context, duration):
    context.test.check_for_num_file_range_generated(1, float('inf'), timeparse(duration))


@then("one flowfile with the contents \"{content}\" is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_multiple_files_generated(1, timeparse(duration), [content])


@then("two flowfiles with the contents \"{content_1}\" and \"{content_2}\" are placed in the monitored directory in less than {duration}")
def step_impl(context, content_1, content_2, duration):
    context.test.check_for_multiple_files_generated(2, timeparse(duration), [content_1, content_2])


@then("flowfiles with these contents are placed in the monitored directory in less than {duration}: \"{contents}\"")
def step_impl(context, duration, contents):
    contents_arr = contents.split(",")
    context.test.check_for_multiple_files_generated(0, timeparse(duration), contents_arr)


@then("after a wait of {duration}, at least {lower_bound:d} and at most {upper_bound:d} flowfiles are produced and placed in the monitored directory")
def step_impl(context, lower_bound, upper_bound, duration):
    context.test.check_for_num_file_range_generated(lower_bound, upper_bound, timeparse(duration))


@then("{number_of_files:d} flowfiles are placed in the monitored directory in {duration}")
@then("{number_of_files:d} flowfile is placed in the monitored directory in {duration}")
def step_impl(context, number_of_files, duration):
    context.test.check_for_multiple_files_generated(number_of_files, timeparse(duration))


@then("at least one empty flowfile is placed in the monitored directory in less than {duration}")
def step_impl(context, duration):
    context.test.check_for_an_empty_file_generated(timeparse(duration))


@then("no files are placed in the monitored directory in {duration} of running time")
def step_impl(context, duration):
    context.test.check_for_no_files_generated(timeparse(duration))


@then("no errors were generated on the http-proxy regarding \"{url}\"")
def step_impl(context, url):
    context.test.check_http_proxy_access('http-proxy', url)


@then("the object on the s3 server is \"{object_data}\"")
def step_impl(context, object_data):
    context.test.check_s3_server_object_data("s3-server", object_data)


@then("the object content type on the s3 server is \"{content_type}\" and the object metadata matches use metadata")
def step_impl(context, content_type):
    context.test.check_s3_server_object_metadata("s3-server", content_type)


@then("the object bucket on the s3 server is empty")
def step_impl(context):
    context.test.check_empty_s3_bucket("s3-server")


# Azure
@when("test blob \"{blob_name}\" with the content \"{content}\" is created on Azure blob storage")
def step_impl(context, blob_name, content):
    context.test.add_test_blob(blob_name, content, False)


@when("test blob \"{blob_name}\" with the content \"{content}\" and a snapshot is created on Azure blob storage")
def step_impl(context, blob_name, content):
    context.test.add_test_blob(blob_name, content, True)


@when("test blob \"{blob_name}\" is created on Azure blob storage")
def step_impl(context, blob_name):
    context.test.add_test_blob(blob_name, "", False)


@when("test blob \"{blob_name}\" is created on Azure blob storage with a snapshot")
def step_impl(context, blob_name):
    context.test.add_test_blob(blob_name, "", True)


@then("the object on the Azure storage server is \"{object_data}\"")
def step_impl(context, object_data):
    context.test.check_azure_storage_server_data("azure-storage-server", object_data)


@then("the Azure blob storage becomes empty in {timeout_seconds:d} seconds")
def step_impl(context, timeout_seconds):
    context.test.check_azure_blob_storage_is_empty(timeout_seconds)


@then("the blob and snapshot count becomes {blob_and_snapshot_count:d} in {timeout_seconds:d} seconds")
def step_impl(context, blob_and_snapshot_count, timeout_seconds):
    context.test.check_azure_blob_and_snapshot_count(blob_and_snapshot_count, timeout_seconds)


# SQL
@then("the query \"{query}\" returns {number_of_rows:d} rows in less than {timeout_seconds:d} seconds on the PostgreSQL server")
def step_impl(context, query, number_of_rows, timeout_seconds):
    context.test.check_query_results("postgresql-server", query, number_of_rows, timeout_seconds)


@then("the Minifi logs contain the following message: \"{log_message}\" in less than {duration}")
def step_impl(context, log_message, duration):
    context.test.check_minifi_log_contents(log_message, timeparse(duration))


@then("the Minifi logs contain the following message: \"{log_message}\" {count:d} times after {seconds:d} seconds")
def step_impl(context, log_message, count, seconds):
    time.sleep(seconds)
    context.test.check_minifi_log_contents(log_message, 1, count)


@then("the Minifi logs do not contain the following message: \"{log_message}\" after {seconds:d} seconds")
def step_impl(context, log_message, seconds):
    context.test.check_minifi_log_does_not_contain(log_message, seconds)


@then("the Minifi logs match the following regex: \"{regex}\" in less than {duration}")
def step_impl(context, regex, duration):
    context.test.check_minifi_log_matches_regex(regex, timeparse(duration))


@then("the OPC UA server logs contain the following message: \"{log_message}\" in less than {duration}")
def step_impl(context, log_message, duration):
    context.test.check_container_log_contents("opcua-server", log_message, timeparse(duration))


# MQTT
@then("the MQTT broker has a log line matching \"{log_pattern}\"")
def step_impl(context, log_pattern):
    context.test.check_container_log_matches_regex('mqtt-broker', log_pattern, 10, count=1)


@then("the MQTT broker has {log_count} log lines matching \"{log_pattern}\"")
def step_impl(context, log_count, log_pattern):
    context.test.check_container_log_matches_regex('mqtt-broker', log_pattern, 10, count=int(log_count))


@then("the \"{minifi_container_name}\" flow has a log line matching \"{log_pattern}\" in less than {duration}")
def step_impl(context, minifi_container_name, log_pattern, duration):
    context.test.check_container_log_matches_regex(minifi_container_name, log_pattern, timeparse(duration), count=1)


@then("an MQTT broker is deployed in correspondence with the PublishMQTT")
def step_impl(context):
    context.test.acquire_container("mqtt-broker", "mqtt-broker")
    context.test.start()


@when("the MQTT broker is started")
def step_impl(context):
    context.test.start('mqtt-broker')


# Google Cloud Storage
@then('an object with the content \"{content}\" is present in the Google Cloud storage')
def step_imp(context, content):
    context.test.check_google_cloud_storage("fake-gcs-server", content)


@then("the test bucket of Google Cloud Storage is empty")
def step_impl(context):
    context.test.check_empty_gcs_bucket("fake-gcs-server")


# Splunk
@then('an event is registered in Splunk HEC with the content \"{content}\"')
def step_imp(context, content):
    context.test.check_splunk_event("splunk", content)


@then('an event is registered in Splunk HEC with the content \"{content}\" with \"{source}\" set as source and \"{source_type}\" set as sourcetype and \"{host}\" set as host')
def step_imp(context, content, source, source_type, host):
    attr = {"source": source, "sourcetype": source_type, "host": host}
    context.test.check_splunk_event_with_attributes("splunk", content, attr)


# Prometheus
@given("a Prometheus server is set up")
def step_impl(context):
    context.test.acquire_container("prometheus", "prometheus")


@then("\"{metric_class}\" are published to the Prometheus server in less than {timeout_seconds:d} seconds")
@then("\"{metric_class}\" is published to the Prometheus server in less than {timeout_seconds:d} seconds")
def step_impl(context, metric_class, timeout_seconds):
    context.test.check_metric_class_on_prometheus(metric_class, timeout_seconds)


@then("\"{metric_class}\" processor metric is published to the Prometheus server in less than {timeout_seconds:d} seconds for \"{processor_name}\" processor")
def step_impl(context, metric_class, timeout_seconds, processor_name):
    context.test.check_processor_metric_on_prometheus(metric_class, timeout_seconds, processor_name)


@then("Elasticsearch is empty")
def step_impl(context):
    context.test.check_empty_elastic("elasticsearch")


@then(u'Elasticsearch has a document with "{doc_id}" in "{index}" that has "{value}" set in "{field}"')
def step_impl(context, doc_id, index, value, field):
    context.test.check_elastic_field_value("elasticsearch", index_name=index, doc_id=doc_id, field_name=field, field_value=value)


@then("Opensearch is empty")
def step_impl(context):
    context.test.check_empty_elastic("opensearch")


@then(u'Opensearch has a document with "{doc_id}" in "{index}" that has "{value}" set in "{field}"')
def step_impl(context, doc_id, index, value, field):
    context.test.check_elastic_field_value("opensearch", index_name=index, doc_id=doc_id, field_name=field, field_value=value)


# MiNiFi C2 Server
@given("a ssl context service is set up for MiNiFi C2 server")
def step_impl(context):
    ssl_context_service = SSLContextService(cert="/tmp/resources/minifi-c2-server-ssl/minifi-cpp-flow.crt", ca_cert="/tmp/resources/minifi-c2-server-ssl/root-ca.pem", key="/tmp/resources/minifi-c2-server-ssl/minifi-cpp-flow.key", passphrase="abcdefgh")
    ssl_context_service.name = "SSLContextService"
    container = context.test.acquire_container("minifi-cpp-flow", "minifi-cpp-with-https-c2-config")
    container.add_controller(ssl_context_service)


@given(u'a MiNiFi C2 server is set up')
def step_impl(context):
    context.test.acquire_container("minifi-c2-server", "minifi-c2-server")


@then("the MiNiFi C2 server logs contain the following message: \"{log_message}\" in less than {duration}")
def step_impl(context, log_message, duration):
    context.test.check_container_log_contents("minifi-c2-server", log_message, timeparse(duration))


@then("the MiNiFi C2 SSL server logs contain the following message: \"{log_message}\" in less than {duration}")
def step_impl(context, log_message, duration):
    context.test.check_container_log_contents("minifi-c2-server-ssl", log_message, timeparse(duration))


@given(u'a MiNiFi C2 server is set up with SSL')
def step_impl(context):
    context.test.acquire_container("minifi-c2-server", "minifi-c2-server-ssl")
