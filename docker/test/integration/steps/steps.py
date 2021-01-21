from MiNiFi_integration_test_driver import MiNiFi_integration_test

from minifi.core.DockerTestCluster import DockerTestCluster
from minifi.core.FileSystemObserver import FileSystemObserver

from minifi.processors.GetFile import GetFile
from minifi.processors.LogAttribute import LogAttribute
from minifi.processors.PutFile import PutFile

from minifi.validators.SingleFileOutputValidator import SingleFileOutputValidator

from behave import *
from pydoc import locate
from pytimeparse.timeparse import timeparse

import os
import logging
import re
import time

@given("a {processor_type} processor with the \"{property}\" property set to \"{property_value}\"")
def step_impl(context, processor_type, property, property_value):
    processor = locate("minifi.processors." + processor_type + "." + processor_type)()
    processor.set_property(property, property_value)
    processor.set_name(processor_type)
    context.test.add_processor(processor)

@given("the \"{relationship}\" relationship of the {source_name} processor is connected to {destination_name}")
def step_impl(context, relationship, source_name, destination_name):
    source = context.test.get_processor_by_name(source_name)
    destination = context.test.get_processor_by_name(destination_name)
    source.out_proc.connect({relationship: destination})

@given("the content of \"{directory}\" is monitored")
def step_impl(context, directory):
    # FIXME: directory is unused???
    output_validator = SingleFileOutputValidator("test")
    # tmp_test_output_dir = "/tmp/.nifi-test-output." + context.test.get_test_id()
    # logging.info("Creating tmp test output dir: %s", tmp_test_output_dir)
    # os.makedirs(tmp_test_output_dir)
    # os.chmod(tmp_test_output_dir, 0o777)
    context.test.add_file_system_observer(FileSystemObserver(context.test.get_output_dir(), output_validator))

@given("a set of processors")
def step_impl(context):
    for row in context.table:
        processor = locate("minifi.processors." + row["type"] + "." + row["type"])()
        processor.set_name(row["name"])
        processor.set_uuid(row["uuid"])
        context.test.add_processor(processor)


@given("these processor properties are set")
def step_impl(context):
    for row in context.table:
        context.test.get_processor_by_name(row["processor name"]).set_property(row["property name"], row["property value"])

@given("the processors are connected up as described here")
def step_impl(context):
    for row in context.table:
        context.execute_steps(
            "given the \"" + row["relationship name"] + "\" relationship of the " + row["source name"] + " processor is connected to " + row["destination name"])

@when("the MiNiFi instance starts up")
def step_impl(context):
    context.test.set_cluster(DockerTestCluster(context.test.get_test_id(), context.test.file_system_observer))
    context.test.add_test_data("test")
    # Assumes that the first processor is the entry point
    context.test.start(context.test.processors[0])

@then("flowfiles are placed in the monitored directory in less than {duration}")
def step_impl(context, duration):
    context.test.check_output(timeparse(duration))
