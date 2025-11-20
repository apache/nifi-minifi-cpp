#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

from behave import given, step

from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.http_proxy_container import HttpProxy
from minifi_test_framework.core.minifi_test_context import DEFAULT_MINIFI_CONTAINER_NAME, MinifiTestContext
from minifi_test_framework.minifi.connection import Connection
from minifi_test_framework.minifi.controller_service import ControllerService
from minifi_test_framework.minifi.funnel import Funnel
from minifi_test_framework.minifi.parameter import Parameter
from minifi_test_framework.minifi.parameter_context import ParameterContext
from minifi_test_framework.minifi.processor import Processor


@given("a transient MiNiFi flow with a LogOnDestructionProcessor processor")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().command = ["/bin/sh", "-c", "timeout 10s ./bin/minifi.sh run && sleep 100"]
    context.get_or_create_default_minifi_container().flow_definition.add_processor(
        Processor("LogOnDestructionProcessor", "LogOnDestructionProcessor"))


@given(
    'a {processor_type} processor with the name "{processor_name}" and the "{property_name}" property set to "{property_value}"')
def step_impl(context: MinifiTestContext, processor_type: str, processor_name: str, property_name: str,
              property_value: str):
    processor = Processor(processor_type, processor_name)
    processor.add_property(property_name, property_value)
    context.get_or_create_default_minifi_container().flow_definition.add_processor(processor)


@step('a {processor_type} processor with the "{property_name}" property set to "{property_value}"')
def step_impl(context: MinifiTestContext, processor_type: str, property_name: str, property_value: str):
    context.execute_steps(
        f'Given a {processor_type} processor with the name "{processor_type}" and the "{property_name}" property set to "{property_value}"')


@step('a {processor_type} processor with the "{property_name}" property set to "{property_value}" in the "{minifi_container_name}" flow')
def step_impl(context: MinifiTestContext, processor_type: str, property_name: str, property_value: str, minifi_container_name: str):
    processor = Processor(processor_type, processor_type)
    processor.add_property(property_name, property_value)
    context.get_or_create_minifi_container(minifi_container_name).flow_definition.add_processor(processor)


@given('a {processor_type} processor with the name "{processor_name}"')
def step_impl(context: MinifiTestContext, processor_type: str, processor_name: str):
    processor = Processor(processor_type, processor_name)
    context.get_or_create_default_minifi_container().flow_definition.add_processor(processor)


@given("a {processor_type} processor in the \"{minifi_container_name}\" flow")
def step_impl(context: MinifiTestContext, processor_type: str, minifi_container_name: str):
    processor = Processor(processor_type, processor_type)
    context.get_or_create_minifi_container(minifi_container_name).flow_definition.add_processor(processor)


@given("a {processor_type} processor")
def step_impl(context: MinifiTestContext, processor_type: str):
    context.execute_steps(f'given a {processor_type} processor in the "{DEFAULT_MINIFI_CONTAINER_NAME}" flow')


@step('the "{property_name}" property of the {processor_name} processor is set to "{property_value}"')
def step_impl(context: MinifiTestContext, property_name: str, processor_name: str, property_value: str):
    processor = context.get_or_create_default_minifi_container().flow_definition.get_processor(processor_name)
    processor.add_property(property_name, property_value)


@step('a Funnel with the name "{funnel_name}" is set up')
def step_impl(context: MinifiTestContext, funnel_name: str):
    context.get_or_create_default_minifi_container().flow_definition.add_funnel(Funnel(funnel_name))


@step('in the "{minifi_container_name}" flow the "{relationship_name}" relationship of the {source} processor is connected to the {target}')
def step_impl(context: MinifiTestContext, relationship_name: str, source: str, target: str, minifi_container_name: str):
    connection = Connection(source_name=source, source_relationship=relationship_name, target_name=target)
    context.get_or_create_minifi_container(minifi_container_name).flow_definition.add_connection(connection)


@step('the "{relationship_name}" relationship of the {source} processor is connected to the {target}')
def step_impl(context: MinifiTestContext, relationship_name: str, source: str, target: str):
    context.execute_steps(f'given in the "{DEFAULT_MINIFI_CONTAINER_NAME}" flow the "{relationship_name}" relationship of the {source} processor is connected to the {target}')


@step('the Funnel with the name "{funnel_name}" is connected to the {target}')
def step_impl(context: MinifiTestContext, funnel_name: str, target: str):
    connection = Connection(source_name=funnel_name, source_relationship="success", target_name=target)
    context.get_or_create_default_minifi_container().flow_definition.add_connection(connection)


@step("{processor_name}'s {relationship} relationship is auto-terminated in the \"{minifi_container_name}\" flow")
def step_impl(context: MinifiTestContext, processor_name: str, relationship: str, minifi_container_name: str):
    context.get_or_create_minifi_container(minifi_container_name).flow_definition.get_processor(processor_name).auto_terminated_relationships.append(
        relationship)


@step("{processor_name}'s {relationship} relationship is auto-terminated")
def step_impl(context: MinifiTestContext, processor_name: str, relationship: str):
    context.execute_steps(f'given {processor_name}\'s {relationship} relationship is auto-terminated in the "{DEFAULT_MINIFI_CONTAINER_NAME}" flow')


@given("a transient MiNiFi flow is set up")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().command = ["/bin/sh", "-c", "timeout 10s ./bin/minifi.sh run && sleep 100"]


@step('the scheduling period of the {processor_name} processor is set to "{duration_str}"')
def step_impl(context: MinifiTestContext, processor_name: str, duration_str: str):
    context.get_or_create_default_minifi_container().flow_definition.get_processor(processor_name).scheduling_period = duration_str


@given("parameter context name is set to '{context_name}'")
def step_impl(context: MinifiTestContext, context_name: str):
    context.get_or_create_default_minifi_container().flow_definition.parameter_contexts.append(ParameterContext(context_name))


@step(
    "a non-sensitive parameter in the flow config called '{parameter_name}' with the value '{parameter_value}' in the parameter context '{context_name}'")
def step_impl(context: MinifiTestContext, parameter_name: str, parameter_value: str, context_name: str):
    parameter_context = context.get_or_create_default_minifi_container().flow_definition.get_parameter_context(context_name)
    parameter_context.parameters.append(Parameter(parameter_name, parameter_value, False))


@step('a directory at "{directory}" has a file with the content "{content}" in the "{flow_name}" flow')
def step_impl(context: MinifiTestContext, directory: str, content: str, flow_name: str):
    new_content = content.replace("\\n", "\n")
    new_dir = Directory(directory)
    new_dir.files["input.txt"] = new_content
    context.get_or_create_minifi_container(flow_name).dirs.append(new_dir)


@step('a directory at "{directory}" has a file with the content "{content}"')
def step_impl(context: MinifiTestContext, directory: str, content: str):
    context.execute_steps(f'given a directory at "{directory}" has a file with the content "{content}" in the "{DEFAULT_MINIFI_CONTAINER_NAME}" flow')


@step('a directory at "{directory}" has a file ("{file_name}") with the content "{content}"')
def step_impl(context: MinifiTestContext, directory: str, file_name: str, content: str):
    new_content = content.replace("\\n", "\n")
    new_dir = Directory(directory)
    new_dir.files[file_name] = new_content
    context.get_or_create_default_minifi_container().dirs.append(new_dir)


@given("these processor properties are set in the \"{minifi_container_name}\" flow")
def step_impl(context: MinifiTestContext, minifi_container_name: str):
    for row in context.table:
        processor = context.get_or_create_minifi_container(minifi_container_name).flow_definition.get_processor(row["processor name"])
        processor.add_property(row["property name"], row["property value"])


@given("these processor properties are set")
def step_impl(context: MinifiTestContext):
    for row in context.table:
        processor = context.get_or_create_default_minifi_container().flow_definition.get_processor(row["processor name"])
        processor.add_property(row["property name"], row["property value"])


@step("the http proxy server is set up")
def step_impl(context):
    context.containers["http-proxy"] = HttpProxy(context)


@step("the processors are connected up as described here")
def step_impl(context: MinifiTestContext):
    for row in context.table:
        source_proc_name = row["source name"]
        dest_proc_name = row["destination name"]
        relationship = row["relationship name"]
        if dest_proc_name == "auto-terminated":
            context.get_or_create_default_minifi_container().flow_definition.get_processor(
                source_proc_name).auto_terminated_relationships.append(relationship)
        else:
            connection = Connection(source_name=row["source name"], source_relationship=relationship,
                                    target_name=row["destination name"])
            context.get_or_create_default_minifi_container().flow_definition.add_connection(connection)


@step("{processor_name} is EVENT_DRIVEN in the \"{minifi_container_name}\" flow")
def step_impl(context: MinifiTestContext, processor_name: str, minifi_container_name: str):
    processor = context.get_or_create_minifi_container(minifi_container_name).flow_definition.get_processor(processor_name)
    processor.scheduling_strategy = "EVENT_DRIVEN"


@step("{processor_name} is EVENT_DRIVEN")
def step_impl(context: MinifiTestContext, processor_name: str):
    context.execute_steps(f'given {processor_name} is EVENT_DRIVEN in the "{DEFAULT_MINIFI_CONTAINER_NAME}" flow')


@step("{processor_name} is TIMER_DRIVEN with {scheduling_period} scheduling period")
def step_impl(context: MinifiTestContext, processor_name: str, scheduling_period: int):
    processor = context.get_or_create_default_minifi_container().flow_definition.get_processor(processor_name)
    processor.scheduling_strategy = "TIMER_DRIVEN"
    processor.scheduling_period = scheduling_period


@given("a {service_name} controller service is set up")
@given("an {service_name} controller service is set up")
def step_impl(context: MinifiTestContext, service_name: str):
    controller_service = ControllerService(class_name=service_name, service_name=service_name)
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(controller_service)


@given('a {service_name} controller service is set up and the "{property_name}" property set to "{property_value}"')
def step_impl(context: MinifiTestContext, service_name: str, property_name: str, property_value: str):
    controller_service = ControllerService(class_name=service_name, service_name=service_name)
    controller_service.add_property(property_name, property_value)
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(controller_service)
