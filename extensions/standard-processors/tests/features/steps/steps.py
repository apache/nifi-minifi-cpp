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

import humanfriendly
from behave import step, then, given

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.helpers import wait_for_condition
from containers.syslog_container import SyslogContainer
from containers.diag_slave_container import DiagSlave
from containers.tcp_client_container import TcpClientContainer

from minifi_test_framework.containers.minifi_protocol import set_up_ssl_properties
from minifi_c2_server_container import MinifiC2Server


@step("a Syslog client with TCP protocol is setup to send logs to minifi")
def setup_syslog_tcp_client(context: MinifiTestContext):
    context.containers["syslog-tcp"] = SyslogContainer("tcp", context)


@step("a Syslog client with UDP protocol is setup to send logs to minifi")
def setup_syslog_udp_client(context: MinifiTestContext):
    context.containers["syslog-udp"] = SyslogContainer("udp", context)


@step('there is an accessible PLC with modbus enabled')
def setup_plc_with_modbus(context: MinifiTestContext):
    modbus_container = context.containers["diag-slave-tcp"] = DiagSlave(context)
    assert modbus_container.deploy(context)


@step('PLC register has been set with {modbus_cmd} command')
def set_plc_register_with_command(context: MinifiTestContext, modbus_cmd: str):
    assert context.containers["diag-slave-tcp"].set_value_on_plc_with_modbus(modbus_cmd) or context.containers["diag-slave-tcp"].log_app_output()


@step('a TCP client is set up to send a test TCP message to minifi')
def setup_tcp_client(context: MinifiTestContext):
    context.containers["tcp-client"] = TcpClientContainer(context)


@given("C2 is enabled in MiNiFi")
def enable_c2_in_minifi(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().set_property("nifi.c2.enable", "true")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.rest.url", f"http://minifi-c2-server-{context.scenario_id}:10090/c2/config/heartbeat")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.rest.url.ack", f"http://minifi-c2-server-{context.scenario_id}:10090/c2/config/acknowledge")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.flow.base.url", f"http://minifi-c2-server-{context.scenario_id}:10090/c2/config/")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation,AssetInformation")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.full.heartbeat", "false")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.agent.class", "minifi-test-class")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.agent.identifier", "minifi-test-id")


@given("ssl properties are set up for MiNiFi C2 server")
def setup_ssl_properties_for_c2(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().set_property("nifi.c2.enable", "true")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.rest.url", f"https://minifi-c2-server-{context.scenario_id}:10090/c2/config/heartbeat")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.rest.url.ack", f"https://minifi-c2-server-{context.scenario_id}:10090/c2/config/acknowledge")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.flow.base.url", f"https://minifi-c2-server-{context.scenario_id}:10090/c2/config/")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation,AssetInformation")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.full.heartbeat", "false")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.agent.class", "minifi-test-class")
    context.get_or_create_default_minifi_container().set_property("nifi.c2.agent.identifier", "minifi-test-id")
    set_up_ssl_properties(context.get_or_create_default_minifi_container())


@given("a MiNiFi C2 server is set up")
def setup_minifi_c2_server(context: MinifiTestContext):
    context.containers["minifi-c2-server"] = MinifiC2Server(context)


@given("a MiNiFi C2 server is set up with SSL")
def setup_minifi_c2_server_with_ssl(context: MinifiTestContext):
    context.containers["minifi-c2-server"] = MinifiC2Server(context, ssl=True)


@given("a MiNiFi C2 server is started")
def start_minifi_c2_server(context: MinifiTestContext):
    context.containers["minifi-c2-server"] = MinifiC2Server(context)
    assert context.containers["minifi-c2-server"].deploy(context)


@then("the MiNiFi C2 server logs contain the following message: \"{log_message}\" in less than {duration}")
def verify_c2_server_logs_contain_message(context: MinifiTestContext, log_message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: log_message in context.containers["minifi-c2-server"].get_logs(),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.containers["minifi-c2-server"].exited,
                              context=context)
