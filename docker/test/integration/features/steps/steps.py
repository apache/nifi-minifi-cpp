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

from filesystem_validation.FileSystemObserver import FileSystemObserver
from minifi.core.RemoteProcessGroup import RemoteProcessGroup
from minifi.core.Funnel import Funnel

from minifi.controllers.SSLContextService import SSLContextService
from minifi.controllers.ODBCService import ODBCService
from minifi.controllers.KubernetesControllerService import KubernetesControllerService
from minifi.controllers.JsonRecordSetWriter import JsonRecordSetWriter
from minifi.controllers.JsonTreeReader import JsonTreeReader
from minifi.controllers.XMLReader import XMLReader
from minifi.controllers.XMLRecordSetWriter import XMLRecordSetWriter
from minifi.controllers.XMLReader import XMLReader

from behave import given, then, when
from behave.model_describe import ModelDescriptor
from pydoc import locate

import logging
import time
import uuid
import humanfriendly

import os


# Background
@given("the content of \"{directory}\" is monitored")
def step_impl(context, directory):
    context.test.add_file_system_observer(FileSystemObserver(context.directory_bindings.docker_path_to_local_path(directory)))


@given("there is a \"{subdir}\" subdirectory in the monitored directory")
def step_impl(context, subdir):
    output_dir = context.test.file_system_observer.get_output_dir() + "/" + subdir
    os.mkdir(output_dir)
    os.chmod(output_dir, 0o777)


def __create_processor(context, processor_type, processor_name, property_name, property_value, container_name, engine='minifi-cpp'):
    container = context.test.acquire_container(context=context, name=container_name, engine=engine)
    processor = locate("minifi.processors." + processor_type + "." + processor_type)(context=context)
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


@given(
    "a {processor_type} processor with the name \"{processor_name}\" and the \"{property_name}\" property set to \"{property_value}\" in the \"{minifi_container_name}\" flow using the \"{engine_name}\" engine")
def step_impl(context, processor_type, processor_name, property_name, property_value, minifi_container_name, engine_name):
    __create_processor(context, processor_type, processor_name, property_name, property_value, minifi_container_name, engine_name)


@given("a {processor_type} processor with the \"{property_name}\" property set to \"{property_value}\" in a \"{minifi_container_name}\" flow")
@given("a {processor_type} processor with the \"{property_name}\" property set to \"{property_value}\" in the \"{minifi_container_name}\" flow")
def step_impl(context, processor_type, property_name, property_value, minifi_container_name):
    __create_processor(context, processor_type, processor_type, property_name, property_value, minifi_container_name)


@given("a {processor_type} processor with the \"{property_name}\" property set to \"{property_value}\" in the \"{minifi_container_name}\" flow using the \"{engine_name}\" engine")
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


@given("a {processor_type} processor with the name \"{processor_name}\" in the \"{minifi_container_name}\" flow using the \"{engine_name}\" engine")
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
@given("a {processor_type} processor set up to communicate with an Azure blob storage")
@given("a {processor_type} processor set up to communicate with an MQTT broker instance")
def step_impl(context, processor_type):
    __create_processor(context, processor_type, processor_type, None, None, "minifi-cpp-flow")


@given("a {processor_type} processor in a Kubernetes cluster")
@given("a {processor_type} processor in the Kubernetes cluster")
def step_impl(context, processor_type):
    __create_processor(context, processor_type, processor_type, None, None, "kubernetes", "kubernetes")


@given("a set of processors in the \"{minifi_container_name}\" flow")
def step_impl(context, minifi_container_name):
    container = context.test.acquire_container(context=context, name=minifi_container_name)
    logging.info(context.table)
    for row in context.table:
        processor = locate("minifi.processors." + row["type"] + "." + row["type"])(context=context)
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


@given("a RemoteProcessGroup node with name \"{rpg_name}\" is opened on \"{address}\" with transport protocol set to \"{transport_protocol}\"")
def step_impl(context, rpg_name, address, transport_protocol):
    remote_process_group = RemoteProcessGroup(address, rpg_name, transport_protocol)
    context.test.add_remote_process_group(remote_process_group)


@given("a RemoteProcessGroup node with name \"{rpg_name}\" is opened on \"{address}\"")
def step_impl(context, rpg_name, address):
    context.execute_steps(f"given a RemoteProcessGroup node with name \"{rpg_name}\" is opened on \"{address}\" with transport protocol set to \"RAW\"")


@given("the \"{property_name}\" property of the {processor_name} processor is set to \"{property_value}\"")
def step_impl(context, property_name, processor_name, property_value):
    processor = context.test.get_node_by_name(processor_name)
    if property_value == "(not set)":
        processor.unset_property(property_name)
    else:
        processor.set_property(property_name, property_value)


@given("the \"{property_name}\" property of the {controller_name} controller is set to \"{property_value}\"")
def step_impl(context, property_name, controller_name, property_value):
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    container.get_controller(controller_name).set_property(property_name, property_value)


@given("the \"{property_name}\" properties of the {processor_name_one} and {processor_name_two} processors are set to the same random guid")
def step_impl(context, property_name, processor_name_one, processor_name_two):
    uuid_str = str(uuid.uuid4())
    context.test.get_node_by_name(processor_name_one).set_property(property_name, uuid_str)
    context.test.get_node_by_name(processor_name_two).set_property(property_name, uuid_str)


@given("the max concurrent tasks attribute of the {processor_name} processor is set to {max_concurrent_tasks:d}")
def step_impl(context, processor_name, max_concurrent_tasks):
    processor = context.test.get_node_by_name(processor_name)
    processor.set_max_concurrent_tasks(max_concurrent_tasks)


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


@given("the scheduling period of the {processor_name} processor is set to \"{scheduling_period}\"")
def step_impl(context, processor_name, scheduling_period):
    processor = context.test.get_node_by_name(processor_name)
    processor.set_scheduling_strategy("TIMER_DRIVEN")
    processor.set_scheduling_period(scheduling_period)


@given("these processor properties are set")
@given("these processor properties are set to match the http proxy")
def step_impl(context):
    for row in context.table:
        context.test.get_node_by_name(row["processor name"]).set_property(row["property name"], row["property value"])


@given("an input port with name \"{port_name}\" is created on the RemoteProcessGroup named \"{rpg_name}\"")
def step_impl(context, port_name, rpg_name):
    remote_process_group = context.test.get_remote_process_group_by_name(rpg_name)
    input_port_node = context.test.generate_input_port_for_remote_process_group(remote_process_group, port_name)
    context.test.add_node(input_port_node)


@given("an input port using compression with name \"{port_name}\" is created on the RemoteProcessGroup named \"{rpg_name}\"")
def step_impl(context, port_name, rpg_name):
    remote_process_group = context.test.get_remote_process_group_by_name(rpg_name)
    input_port_node = context.test.generate_input_port_for_remote_process_group(remote_process_group, port_name, True)
    context.test.add_node(input_port_node)


@given("an output port with name \"{port_name}\" is created on the RemoteProcessGroup named \"{rpg_name}\"")
def step_impl(context, port_name, rpg_name):
    remote_process_group = context.test.get_remote_process_group_by_name(rpg_name)
    input_port_node = context.test.generate_output_port_for_remote_process_group(remote_process_group, port_name)
    context.test.add_node(input_port_node)


@given("an output port using compression with name \"{port_name}\" is created on the RemoteProcessGroup named \"{rpg_name}\"")
def step_impl(context, port_name, rpg_name):
    remote_process_group = context.test.get_remote_process_group_by_name(rpg_name)
    input_port_node = context.test.generate_output_port_for_remote_process_group(remote_process_group, port_name, True)
    context.test.add_node(input_port_node)


@given("the output port \"{port_name}\" is connected to the {destination_name} processor")
def step_impl(context, port_name, destination_name):
    destination = context.test.get_node_by_name(destination_name)
    output_port_node = context.test.get_node_by_name(port_name)
    output_port_node.out_proc.connect({"undefined": destination})


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
@given("a file with the content '{content}' is present in '{path}'")
@then("a file with the content \"{content}\" is placed in \"{path}\"")
def step_impl(context, content, path):
    context.test.add_test_data(path, content)


@given("a file of size {size} is present in \"{path}\"")
def step_impl(context, size: str, path: str):
    context.test.add_random_test_data(path, humanfriendly.parse_size(size))


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
@given("\"{processor_name}\" port is a start node")
def step_impl(context, processor_name):
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    processor = context.test.get_or_create_node_by_name(processor_name)
    container.add_start_node(processor)


# NiFi setups
@given("a NiFi flow is receiving data from the RemoteProcessGroup named \"{rpg_name}\" in an input port named \"{input_port_name}\" which has the same id as the port named \"{rpg_port_name}\"")
def step_impl(context, input_port_name, rpg_port_name, rpg_name):
    remote_process_group = context.test.get_remote_process_group_by_name(rpg_name)
    source = context.test.generate_input_port_for_remote_process_group(remote_process_group, input_port_name)
    source.instance_id = context.test.get_node_by_name(rpg_port_name).instance_id
    context.test.add_node(source)
    container = context.test.acquire_container(context=context, name='nifi', engine='nifi')
    # Assume that the first node declared is primary unless specified otherwise
    if not container.get_start_nodes():
        container.add_start_node(source)


@given("a NiFi flow is sending data to an output port named \"{port_name}\" with the id of the port named \"{rpg_port_name}\" from the RemoteProcessGroup named \"{rpg_name}\"")
def step_impl(context, port_name, rpg_port_name, rpg_name):
    remote_process_group = context.test.get_remote_process_group_by_name(rpg_name)
    destination = context.test.generate_output_port_for_remote_process_group(remote_process_group, port_name)
    destination.instance_id = context.test.get_node_by_name(rpg_port_name).instance_id
    context.test.add_node(destination)


@given("a NiFi flow with the name \"{flow_name}\" is set up")
def step_impl(context, flow_name):
    context.test.acquire_container(context=context, name=flow_name, engine='nifi')


@given("SSL is enabled in NiFi flow")
def step_impl(context):
    context.test.enable_ssl_in_nifi()


@given("a transient MiNiFi flow with the name \"{flow_name}\" is set up")
def step_impl(context, flow_name):
    context.test.acquire_transient_minifi(context=context, name=flow_name)


@given("the provenance repository is enabled in MiNiFi")
def step_impl(context):
    context.test.enable_provenance_repository_in_minifi()


@given("C2 is enabled in MiNiFi")
def step_impl(context):
    context.test.enable_c2_in_minifi()


@given("Prometheus is enabled in MiNiFi")
def step_impl(context):
    context.test.enable_prometheus_in_minifi()


@given("log metrics publisher is enabled in MiNiFi")
def step_impl(context):
    context.test.enable_log_metrics_publisher_in_minifi()


@given("Prometheus with SSL is enabled in MiNiFi")
def step_impl(context):
    context.test.enable_prometheus_with_ssl_in_minifi()


@given("OpenSSL FIPS mode is enabled in MiNiFi")
def step_impl(context):
    context.test.enable_openssl_fips_mode_in_minifi()


@given("OpenSSL FIPS mode is disabled in MiNiFi")
def step_impl(context):
    context.test.disable_openssl_fips_mode_in_minifi()


# HTTP proxy setup
@given("the http proxy server is set up")
@given("a http proxy server is set up accordingly")
def step_impl(context):
    context.test.acquire_container(context=context, name="http-proxy", engine="http-proxy")


# TLS
@given("an ssl context service is set up for {processor_name}")
@given("an ssl context service with a manual CA cert file is set up for {processor_name}")
def step_impl(context, processor_name):
    ssl_context_service = SSLContextService(cert='/tmp/resources/minifi_client.crt',
                                            key='/tmp/resources/minifi_client.key',
                                            ca_cert='/tmp/resources/root_ca.crt')

    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(ssl_context_service)
    processor.set_property('SSL Context Service', ssl_context_service.name)


@given("an ssl context service using the system CA cert store is set up for {processor_name}")
def step_impl(context, processor_name):
    ssl_context_service = SSLContextService(cert='/tmp/resources/minifi_client.crt',
                                            key='/tmp/resources/minifi_client.key',
                                            use_system_cert_store='true')

    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(ssl_context_service)
    processor.set_property('SSL Context Service', ssl_context_service.name)


# Record set reader and writer
@given("a JsonRecordSetWriter controller service is set up with \"{}\" output grouping in the \"{minifi_container_name}\" flow")
def step_impl(context, output_grouping: str, minifi_container_name: str):
    json_record_set_writer = JsonRecordSetWriter(name="JsonRecordSetWriter", output_grouping=output_grouping)
    container = context.test.acquire_container(context=context, name=minifi_container_name)
    container.add_controller(json_record_set_writer)


@given("a JsonTreeReader controller service is set up in the \"{minifi_container_name}\" flow")
def step_impl(context, minifi_container_name: str):
    json_record_set_reader = JsonTreeReader("JsonTreeReader")
    container = context.test.acquire_container(context=context, name=minifi_container_name)
    container.add_controller(json_record_set_reader)


@given("a JsonRecordSetWriter controller service is set up with \"{}\" output grouping")
def step_impl(context, output_grouping: str):
    context.execute_steps(f"given a JsonRecordSetWriter controller service is set up with \"{output_grouping}\" output grouping in the \"minifi-cpp-flow\" flow")


@given("a JsonTreeReader controller service is set up")
def step_impl(context):
    context.execute_steps("given a JsonTreeReader controller service is set up in the \"minifi-cpp-flow\" flow")


@given("a XMLReader controller service is set up")
def step_impl(context):
    xml_reader = XMLReader("XMLReader")
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    container.add_controller(xml_reader)


@given("a XMLRecordSetWriter controller service is set up")
def step_impl(context):
    xml_record_set_writer = XMLRecordSetWriter("XMLRecordSetWriter")
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    container.add_controller(xml_record_set_writer)


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


# MQTT setup
@when("an MQTT broker is set up in correspondence with the PublishMQTT")
@given("an MQTT broker is set up in correspondence with the PublishMQTT")
@given("an MQTT broker is set up in correspondence with the ConsumeMQTT")
@given("an MQTT broker is set up in correspondence with the PublishMQTT and ConsumeMQTT")
def step_impl(context):
    context.test.acquire_container(context=context, name="mqtt-broker", engine="mqtt-broker")
    context.test.start('mqtt-broker')


# azure storage setup
@given("an Azure storage server is set up")
def step_impl(context):
    context.test.acquire_container(context=context, name="azure-storage-server", engine="azure-storage-server")


# syslog client
@given(u'a Syslog client with {protocol} protocol is setup to send logs to minifi')
def step_impl(context, protocol):
    client_name = "syslog-" + protocol.lower() + "-client"
    context.test.acquire_container(context=context, name=client_name, engine=client_name)


def setUpSslContextServiceForProcessor(context, processor_name: str):
    minifi_crt_file = '/tmp/resources/minifi_client.crt'
    minifi_key_file = '/tmp/resources/minifi_client.key'
    root_ca_crt_file = '/tmp/resources/root_ca.crt'
    ssl_context_service = SSLContextService(cert=minifi_crt_file, ca_cert=root_ca_crt_file, key=minifi_key_file)
    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(ssl_context_service)
    processor.set_property("SSL Context Service", ssl_context_service.name)


def setUpSslContextServiceForRPG(context, rpg_name: str):
    minifi_crt_file = '/tmp/resources/minifi_client.crt'
    minifi_key_file = '/tmp/resources/minifi_client.key'
    root_ca_crt_file = '/tmp/resources/root_ca.crt'
    ssl_context_service = SSLContextService(cert=minifi_crt_file, ca_cert=root_ca_crt_file, key=minifi_key_file)
    container = context.test.acquire_container(context=context, name="minifi-cpp-flow")
    container.add_controller(ssl_context_service)
    rpg = context.test.get_remote_process_group_by_name(rpg_name)
    rpg.add_property("SSL Context Service", ssl_context_service.name)


# TCP client
@given('a TCP client is set up to send a test TCP message to minifi')
def step_impl(context):
    context.test.acquire_container(context=context, name="tcp-client", engine="tcp-client")


# SQL
@given("an ODBCService is setup up for {processor_name} with the name \"{service_name}\"")
def step_impl(context, processor_name, service_name):
    odbc_service = ODBCService(name=service_name,
                               connection_string="Driver={{PostgreSQL ANSI}};Server={server_hostname};Port=5432;Database=postgres;Uid=postgres;Pwd=password;".format(server_hostname=context.test.get_container_name_with_postfix("postgresql-server")))
    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(odbc_service)
    processor.set_property("DB Controller Service", odbc_service.name)


@given("a PostgreSQL server is set up")
def step_impl(context):
    context.test.enable_sql_in_minifi()
    context.test.acquire_container(context=context, name="postgresql-server", engine="postgresql-server")


@when("the MiNiFi instance starts up")
@when("both instances start up")
@when("all instances start up")
@when("all other processes start up")
def step_impl(context):
    context.test.start()


@when("\"{container_name}\" flow is stopped")
def step_impl(context, container_name):
    context.test.stop(container_name)


@when("\"{container_name}\" flow is restarted")
def step_impl(context, container_name):
    context.test.restart(container_name)


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


@then("{duration} later")
def step_impl(context, duration):
    time.sleep(humanfriendly.parse_timespan(duration))


@when("content \"{content}\" is added to file \"{file_name}\" present in directory \"{path}\" {seconds:d} seconds later")
def step_impl(context, content, file_name, path, seconds):
    time.sleep(seconds)
    context.test.add_test_data(path, content, file_name)


@then("a flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}")
@then("a flowfile with the content '{content}' is placed in the monitored directory in less than {duration}")
@then("{number_of_flow_files:d} flowfiles with the content \"{content}\" are placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration, number_of_flow_files=1):
    context.test.check_for_multiple_files_generated(number_of_flow_files, humanfriendly.parse_timespan(duration), [content])


@then("a flowfile with the JSON content \"{content}\" is placed in the monitored directory in less than {duration}")
@then("a flowfile with the JSON content '{content}' is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_single_json_file_with_content_generated(content, humanfriendly.parse_timespan(duration))


@then("at least one flowfile's content match the following regex: \"{regex}\" in less than {duration}")
@then("at least one flowfile's content match the following regex: '{regex}' in less than {duration}")
def step_impl(context, regex: str, duration: str):
    context.test.check_for_at_least_one_file_with_matching_content(regex, humanfriendly.parse_timespan(duration))


@then("at least one flowfile with the content \"{content}\" is placed in the monitored directory in less than {duration}")
@then("at least one flowfile with the content '{content}' is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_at_least_one_file_with_content_generated(content, humanfriendly.parse_timespan(duration))


@then("no files are placed in the monitored directory in {duration} of running time")
def step_impl(context, duration):
    context.test.check_for_no_files_generated(humanfriendly.parse_timespan(duration))


@then("there is exactly {num_flowfiles} files in the monitored directory")
def step_impl(context, num_flowfiles):
    context.test.check_for_num_files_generated(int(num_flowfiles), humanfriendly.parse_timespan("1"))


@then("{num_flowfiles} flowfiles are placed in the monitored directory in less than {duration}")
def step_impl(context, num_flowfiles, duration):
    if num_flowfiles == 0:
        context.execute_steps(f"no files are placed in the monitored directory in {duration} of running time")
        return
    context.test.check_for_num_files_generated(int(num_flowfiles), humanfriendly.parse_timespan(duration))


@then("at least one flowfile is placed in the monitored directory in less than {duration}")
def step_impl(context, duration):
    context.test.check_for_num_file_range_generated_with_timeout(1, float('inf'), humanfriendly.parse_timespan(duration))


@then("at least one flowfile with minimum size of \"{size}\" is placed in the monitored directory in less than {duration}")
def step_impl(context, duration: str, size: str):
    context.test.check_for_num_file_range_and_min_size_generated(1, float('inf'), humanfriendly.parse_size(size), humanfriendly.parse_timespan(duration))


@then("one flowfile with the contents \"{content}\" is placed in the monitored directory in less than {duration}")
def step_impl(context, content, duration):
    context.test.check_for_multiple_files_generated(1, humanfriendly.parse_timespan(duration), [content])


@then("two flowfiles with the contents \"{content_1}\" and \"{content_2}\" are placed in the monitored directory in less than {duration}")
@then("two flowfiles with the contents '{content_1}' and '{content_2}' are placed in the monitored directory in less than {duration}")
def step_impl(context, content_1, content_2, duration):
    context.test.check_for_multiple_files_generated(2, humanfriendly.parse_timespan(duration), [content_1, content_2])


@then("exactly these flowfiles are in the monitored directory in less than {duration}: \"\"")
def step_impl(context, duration):
    context.execute_steps(f"Then no files are placed in the monitored directory in {duration} of running time")


@then("exactly these flowfiles are in the monitored directory's \"{subdir}\" subdirectory in less than {duration}: \"\"")
def step_impl(context, duration, subdir):
    assert context.test.check_subdirectory(sub_directory=subdir, expected_contents=[], timeout=humanfriendly.parse_timespan(duration)) or context.test.cluster.log_app_output()


@then("exactly these flowfiles are in the monitored directory in less than {duration}: \"{contents}\"")
def step_impl(context, duration, contents):
    contents_arr = contents.split(",")
    context.test.check_for_multiple_files_generated(len(contents_arr), humanfriendly.parse_timespan(duration), contents_arr)


@then("exactly these flowfiles are in the monitored directory's \"{subdir}\" subdirectory in less than {duration}: \"{contents}\"")
def step_impl(context, duration, subdir, contents):
    contents_arr = contents.split(",")
    assert context.test.check_subdirectory(sub_directory=subdir, expected_contents=contents_arr, timeout=humanfriendly.parse_timespan(duration)) or context.test.cluster.log_app_output()


@then("flowfiles with these contents are placed in the monitored directory in less than {duration}: \"{contents}\"")
def step_impl(context, duration, contents):
    contents_arr = contents.split(",")
    context.test.check_for_multiple_files_generated(0, humanfriendly.parse_timespan(duration), contents_arr)


@then("after a wait of {duration}, at least {lower_bound:d} and at most {upper_bound:d} flowfiles are produced and placed in the monitored directory")
def step_impl(context, lower_bound, upper_bound, duration):
    context.test.check_for_num_file_range_generated_after_wait(lower_bound, upper_bound, humanfriendly.parse_timespan(duration))


@then("{number_of_files:d} flowfiles are placed in the monitored directory in {duration}")
@then("{number_of_files:d} flowfile is placed in the monitored directory in {duration}")
def step_impl(context, number_of_files, duration):
    context.test.check_for_multiple_files_generated(number_of_files, humanfriendly.parse_timespan(duration))


@then("at least one empty flowfile is placed in the monitored directory in less than {duration}")
def step_impl(context, duration):
    context.test.check_for_an_empty_file_generated(humanfriendly.parse_timespan(duration))


@then("no errors were generated on the http-proxy regarding \"{url}\"")
def step_impl(context, url):
    context.test.check_http_proxy_access('http-proxy', url)


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
def step_impl(context, query: str, number_of_rows: int, timeout_seconds: int):
    context.test.check_query_results(context.test.get_container_name_with_postfix("postgresql-server"), query, number_of_rows, timeout_seconds)


@then("the Minifi logs contain the following message: \"{log_message}\" in less than {duration}")
@then("the Minifi logs contain the following message: '{log_message}' in less than {duration}")
def step_impl(context, log_message, duration):
    context.test.check_minifi_log_contents(log_message, humanfriendly.parse_timespan(duration))


@then("the Minifi logs contain the following message: \"{log_message}\" {count:d} times after {seconds:d} seconds")
def step_impl(context, log_message, count, seconds):
    time.sleep(seconds)
    context.test.check_minifi_log_contents(log_message, 1, count)


@then("the Minifi logs do not contain the following message: \"{log_message}\" after {seconds:d} seconds")
def step_impl(context, log_message, seconds):
    context.test.check_minifi_log_does_not_contain(log_message, seconds)


@then("the Minifi logs match the following regex: \"{regex}\" in less than {duration}")
def step_impl(context, regex, duration):
    context.test.check_minifi_log_matches_regex(regex, humanfriendly.parse_timespan(duration))


# MQTT
@then("the MQTT broker has a log line matching \"{log_pattern}\"")
def step_impl(context, log_pattern):
    context.test.check_container_log_matches_regex('mqtt-broker', log_pattern, 60, count=1)


@then("the MQTT broker has {log_count} log lines matching \"{log_pattern}\"")
def step_impl(context, log_count, log_pattern):
    context.test.check_container_log_matches_regex('mqtt-broker', log_pattern, 60, count=int(log_count))


@when("a test message \"{message}\" is published to the MQTT broker on topic \"{topic}\"")
def step_impl(context, message, topic):
    context.test.publish_test_mqtt_message(topic, message)


@then("the \"{minifi_container_name}\" flow has a log line matching \"{log_pattern}\" in less than {duration}")
def step_impl(context, minifi_container_name, log_pattern, duration):
    context.test.check_container_log_matches_regex(minifi_container_name, log_pattern, humanfriendly.parse_timespan(duration), count=1)


# Prometheus
@given("a Prometheus server is set up")
def step_impl(context):
    context.test.acquire_container(context=context, name="prometheus", engine="prometheus")


@given("a Prometheus server is set up with SSL")
def step_impl(context):
    context.test.acquire_container(context=context, name="prometheus", engine="prometheus-ssl")


@then("\"{metric_class}\" are published to the Prometheus server in less than {timeout_seconds:d} seconds")
@then("\"{metric_class}\" is published to the Prometheus server in less than {timeout_seconds:d} seconds")
def step_impl(context, metric_class, timeout_seconds):
    context.test.check_metric_class_on_prometheus(metric_class, timeout_seconds)


@then("\"{metric_class}\" processor metric is published to the Prometheus server in less than {timeout_seconds:d} seconds for \"{processor_name}\" processor")
def step_impl(context, metric_class, timeout_seconds, processor_name):
    context.test.check_processor_metric_on_prometheus(metric_class, timeout_seconds, processor_name)


@then("all Prometheus metric types are only defined once")
def step_impl(context):
    context.test.check_all_prometheus_metric_types_are_defined_once()


@given("SSL properties are set in MiNiFi")
def step_impl(context):
    context.test.set_ssl_context_properties_in_minifi()


@given(u'flow configuration path is set up in flow url property')
def step_impl(context):
    context.test.acquire_container(context=context, name="minifi-cpp-flow", engine="minifi-cpp")
    context.test.fetch_flow_config_from_c2_url_in_minifi()


# MiNiFi memory usage
@then(u'the peak memory usage of the agent is more than {size} in less than {duration}')
def step_impl(context, size: str, duration: str) -> None:
    context.test.check_if_peak_memory_usage_exceeded(humanfriendly.parse_size(size), humanfriendly.parse_timespan(duration))


@then(u'the memory usage of the agent is less than {size} in less than {duration}')
def step_impl(context, size: str, duration: str) -> None:
    context.test.check_if_memory_usage_is_below(humanfriendly.parse_size(size), humanfriendly.parse_timespan(duration))


@then(u'the memory usage of the agent decreases to {peak_usage_percent}% peak usage in less than {duration}')
def step_impl(context, peak_usage_percent: str, duration: str) -> None:
    context.test.check_memory_usage_compared_to_peak(float(peak_usage_percent) * 0.01, humanfriendly.parse_timespan(duration))


@given(u'a MiNiFi CPP server with yaml config')
def step_impl(context):
    context.test.set_yaml_in_minifi()


@given(u'a MiNiFi CPP server with json config')
def step_impl(context):
    context.test.set_json_in_minifi()


@given(u'a SSL context service is set up for the following processor: \"{processor_name}\"')
def step_impl(context, processor_name: str):
    setUpSslContextServiceForProcessor(context, processor_name)


@given(u'a SSL context service is set up for the following remote process group: \"{remote_process_group}\"')
def step_impl(context, remote_process_group: str):
    setUpSslContextServiceForRPG(context, remote_process_group)


# Python
@given("python with langchain is installed on the MiNiFi agent {install_mode}")
def step_impl(context, install_mode):
    if install_mode == "with required python packages":
        context.test.use_nifi_python_processors_with_system_python_packages_installed_in_minifi()
    elif install_mode == "with a pre-created virtualenv":
        context.test.use_nifi_python_processors_with_virtualenv_in_minifi()
    elif install_mode == "with a pre-created virtualenv containing the required python packages":
        context.test.use_nifi_python_processors_with_virtualenv_packages_installed_in_minifi()
    elif install_mode == "using inline defined Python dependencies to install packages":
        context.test.remove_python_requirements_txt_in_minifi()
    else:
        raise Exception("Unknown python install mode.")


@given("python processors without dependencies are present on the MiNiFi agent")
def step_impl(context):
    context.test.use_nifi_python_processors_without_dependencies_in_minifi()


@given("the example MiNiFi python processors are present")
def step_impl(context):
    context.test.enable_example_minifi_python_processors()


@given("a non-sensitive parameter in the flow config called '{parameter_name}' with the value '{parameter_value}' in the parameter context '{parameter_context_name}'")
def step_impl(context, parameter_context_name, parameter_name, parameter_value):
    container = context.test.acquire_container(context=context, name='minifi-cpp-flow', engine='minifi-cpp')
    container.add_parameter_to_flow_config(parameter_context_name, parameter_name, parameter_value)


@given("parameter context name is set to '{parameter_context_name}'")
def step_impl(context, parameter_context_name):
    container = context.test.acquire_container(context=context, name='minifi-cpp-flow', engine='minifi-cpp')
    container.set_parameter_context_name(parameter_context_name)


@given("a LlamaCpp model is present on the MiNiFi host")
def step_impl(context):
    context.test.llama_model_is_downloaded_in_minifi()


@when(u'NiFi is started')
def step_impl(context):
    context.test.start_nifi(context)
