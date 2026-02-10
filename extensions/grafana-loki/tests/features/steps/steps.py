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
from behave import step, then

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.helpers import log_due_to_failure
from containers.grafana_loki_container import GrafanaLokiContainer, GrafanaLokiOptions
from containers.reverse_proxy_container import ReverseProxyContainer


@step("a Grafana Loki server is set up")
def step_impl(context: MinifiTestContext):
    context.containers["grafana-loki-server"] = GrafanaLokiContainer(context, GrafanaLokiOptions())


@step("a Grafana Loki server is set up with multi-tenancy enabled")
def step_impl(context: MinifiTestContext):
    context.containers["grafana-loki-server"] = GrafanaLokiContainer(context, GrafanaLokiOptions(enable_multi_tenancy=True))


@step("a Grafana Loki server with SSL is set up")
def step_impl(context: MinifiTestContext):
    context.containers["grafana-loki-server"] = GrafanaLokiContainer(context, GrafanaLokiOptions(enable_ssl=True))


@then("\"{lines}\" lines are published to the Grafana Loki server in less than {timeout_seconds:d} seconds")
@then("\"{lines}\" line is published to the Grafana Loki server in less than {timeout_seconds:d} seconds")
def step_impl(context, lines: str, timeout_seconds: int):
    assert context.containers["grafana-loki-server"].are_lines_present(lines, timeout_seconds, ssl=False) or log_due_to_failure(context)


@then("\"{lines}\" lines are published to the \"{tenant_id}\" tenant on the Grafana Loki server in less than {timeout_seconds:d} seconds")
@then("\"{lines}\" line is published to the \"{tenant_id}\" tenant on the Grafana Loki server in less than {timeout_seconds:d} seconds")
def step_impl(context, lines: str, timeout_seconds: int, tenant_id: str):
    assert context.containers["grafana-loki-server"].are_lines_present(lines, timeout_seconds, ssl=False, tenant_id=tenant_id) or log_due_to_failure(context)


@then("\"{lines}\" lines are published using SSL to the Grafana Loki server in less than {timeout_seconds:d} seconds")
@then("\"{lines}\" line is published using SSL to the Grafana Loki server in less than {timeout_seconds:d} seconds")
def step_impl(context, lines: str, timeout_seconds: int):
    assert context.containers["grafana-loki-server"].are_lines_present(lines, timeout_seconds, ssl=True) or log_due_to_failure(context)


# Nginx reverse proxy
@step('a reverse proxy is set up to forward requests to the Grafana Loki server')
def step_impl(context):
    context.containers["reverse-proxy"] = ReverseProxyContainer(context)
