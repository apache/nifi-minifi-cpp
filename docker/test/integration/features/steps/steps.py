from MiNiFi_integration_test_driver import MiNiFi_integration_test

from minifi.core.DockerTestCluster import DockerTestCluster

from minifi.processors.GetFile import GetFile
from minifi.processors.LogAttribute import LogAttribute
from minifi.processors.PutFile import PutFile

from minifi.validators.SingleFileOutputValidator import SingleFileOutputValidator

from behave import *
from pydoc import locate
# from pytimeparse.timeparse import timeparse

import os
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
    context.flow = GetFile('/tmp/input') >> LogAttribute() >> PutFile('/tmp/output')

@when("the MiNiFi instance starts up")
def step_impl(context):
    context.test.set_cluster(DockerTestCluster(SingleFileOutputValidator('test')))
    context.test.add_test_data('test')
    context.test.start(context.flow)

@then("flowfiles are produced in \"{output_directory}\" in less than {duration}")
def step_impl(context, output_directory, duration):
    context.test.check_output()  
