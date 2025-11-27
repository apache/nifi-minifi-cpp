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

from behave import step

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from syslog_container import SyslogContainer
from diag_slave_container import DiagSlave
from tcp_client_container import TcpClientContainer


@step("a Syslog client with TCP protocol is setup to send logs to minifi")
def step_impl(context: MinifiTestContext):
    context.containers["syslog-tcp"] = SyslogContainer("tcp", context)


@step("a Syslog client with UDP protocol is setup to send logs to minifi")
def step_impl(context: MinifiTestContext):
    context.containers["syslog-udp"] = SyslogContainer("udp", context)


@step('there is an accessible PLC with modbus enabled')
def step_impl(context: MinifiTestContext):
    modbus_container = context.containers["diag-slave-tcp"] = DiagSlave(context)
    assert modbus_container.deploy()


@step('PLC register has been set with {modbus_cmd} command')
def step_impl(context: MinifiTestContext, modbus_cmd: str):
    assert context.containers["diag-slave-tcp"].set_value_on_plc_with_modbus(modbus_cmd) or context.containers["diag-slave-tcp"].log_app_output()


@step('a TCP client is set up to send a test TCP message to minifi')
def step_impl(context: MinifiTestContext):
    context.containers["tcp-client"] = TcpClientContainer(context)
