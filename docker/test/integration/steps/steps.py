from MiNiFi_integration_test_driver import MiNiFi_integration_test

from minifi.core.DockerTestCluster import DockerTestCluster
from minifi.core.FileSystemObserver import FileSystemObserver
from minifi.core.RemoteProcessGroup import RemoteProcessGroup
from minifi.core.InputPort import InputPort

from minifi.processors.GetFile import GetFile
from minifi.processors.LogAttribute import LogAttribute
from minifi.processors.PutFile import PutFile

from minifi.validators.SingleFileOutputValidator import SingleFileOutputValidator

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
    output_validator = SingleFileOutputValidator("test")
    context.test.add_file_system_observer(FileSystemObserver(context.test.docker_path_to_local_path(directory), output_validator))

# MiNiFi cluster setups

@given("a {processor_type} processor with the \"{property}\" property set to \"{property_value}\" in a \"{cluster_name}\" flow")
@given("a {processor_type} processor with the \"{property}\" property set to \"{property_value}\" in the \"{cluster_name}\" flow")
def step_impl(context, processor_type, property, property_value, cluster_name):
    logging.info("Acquiring " + cluster_name)
    cluster = context.test.acquire_cluster(cluster_name)
    processor = locate("minifi.processors." + processor_type + "." + processor_type)()
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

@given("the \"{property_name}\" of the {processor_name} processor is set to \"{property_value}\"")
def step_impl(context, property_name, processor_name, property_value):
    processor = context.test.get_node_by_name(processor_name)
    processor.set_property(property_name, property_value)

@given("these processor properties are set")
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

@given("a http proxy server \"{cluster_name}\" is set up accordingly")
def step_impl(context, cluster_name):
    cluster = context.test.acquire_cluster(cluster_name)
    cluster.set_name(cluster_name)
    cluster.set_engine("http-proxy")
    cluster.set_flow(None)

@when("the MiNiFi instance starts up")
@when("both instances start up")
@when("all instances start up")
def step_impl(context):
    # TODO: extract addition of test data
    context.test.add_test_data("test")
    context.test.start()

@then("flowfiles are placed in the monitored directory in less than {duration}")
def step_impl(context, duration):
    context.test.check_output(timeparse(duration))

@then("no errors were generated on the \"{cluster_name}\" regarding \"{url}\"")
def step_impl(context, cluster_name, url):
    context.test.check_http_proxy_access(cluster_name, url)
