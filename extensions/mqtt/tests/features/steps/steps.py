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

import re
from behave import given, step, then, when

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import DEFAULT_MINIFI_CONTAINER_NAME, MinifiTestContext
from minifi_test_framework.minifi.processor import Processor
from minifi_test_framework.core.helpers import wait_for_condition

from mqtt_broker_container import MqttBrokerContainer


@given("a {processor_name} processor set up to communicate with an MQTT broker instance in the \"{container_name}\" flow")
def step_impl(context: MinifiTestContext, processor_name: str, container_name: str):
    processor = Processor(processor_name, processor_name)
    processor.add_property('Broker URI', f'mqtt-broker-{context.scenario_id}:1883')
    processor.add_property('Topic', 'testtopic')
    if processor_name == 'PublishMQTT':
        processor.add_property('Client ID', 'publisher-client')
    elif processor_name == 'ConsumeMQTT':
        processor.add_property('Client ID', 'consumer-client')
    else:
        raise ValueError(f"Unknown processor to communicate with MQTT broker: {processor_name}")

    context.get_or_create_minifi_container(container_name).flow_definition.add_processor(processor)


@given("a {processor_name} processor set up to communicate with an MQTT broker instance")
def step_impl(context: MinifiTestContext, processor_name: str):
    context.execute_steps(f'given a {processor_name} processor set up to communicate with an MQTT broker instance in the "{DEFAULT_MINIFI_CONTAINER_NAME}" flow')


@step("an MQTT broker is started")
def step_impl(context: MinifiTestContext):
    context.containers["mqtt-broker"] = MqttBrokerContainer(context)
    assert context.containers["mqtt-broker"].deploy()


@then('the MQTT broker has a log line matching "{log_line_regex}"')
def step_impl(context: MinifiTestContext, log_line_regex: str):
    assert wait_for_condition(
        condition=lambda: re.search(log_line_regex, context.containers["mqtt-broker"].get_logs()),
        timeout_seconds=60,
        bail_condition=lambda: context.containers["mqtt-broker"].exited,
        context=None)


@then('the MQTT broker has {log_count:d} log lines matching "{log_line_regex}"')
def step_impl(context: MinifiTestContext, log_count: int, log_line_regex: str):
    assert wait_for_condition(
        condition=lambda: len(re.findall(log_line_regex, context.containers["mqtt-broker"].get_logs())) == log_count,
        timeout_seconds=60,
        bail_condition=lambda: context.containers["mqtt-broker"].exited,
        context=None)


@when("a test message \"{message}\" is published to the MQTT broker on topic \"{topic}\"")
def step_impl(context: MinifiTestContext, message: str, topic: str):
    assert context.containers["mqtt-broker"].publish_mqtt_message(topic, message)
