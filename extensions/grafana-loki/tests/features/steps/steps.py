# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
from behave import step, then
from containers.grafana_loki_container import GrafanaLokiContainer, GrafanaLokiOptions
from containers.reverse_proxy_container import ReverseProxyContainer
from minifi_behave.core.helpers import log_due_to_failure
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.steps import (
    checking_steps,  # noqa: F401
    configuration_steps,  # noqa: F401
    core_steps,  # noqa: F401
    flow_building_steps,  # noqa: F401
)


@step("a Grafana Loki server is set up")
def setup_grafana_loki_server(context: MinifiTestContext):
    context.containers["grafana-loki-server"] = GrafanaLokiContainer(context, GrafanaLokiOptions())


@step("a Grafana Loki server is set up with multi-tenancy enabled")
def setup_grafana_loki_server_multitenant(context: MinifiTestContext):
    context.containers["grafana-loki-server"] = GrafanaLokiContainer(
        context, GrafanaLokiOptions(enable_multi_tenancy=True)
    )


@step("a Grafana Loki server with SSL is set up")
def setup_grafana_loki_server_ssl(context: MinifiTestContext):
    context.containers["grafana-loki-server"] = GrafanaLokiContainer(context, GrafanaLokiOptions(enable_ssl=True))


@then('"{lines}" lines are published to the Grafana Loki server in less than {timeout_seconds:d} seconds')
@then('"{lines}" line is published to the Grafana Loki server in less than {timeout_seconds:d} seconds')
def verify_lines_published_to_loki(context, lines: str, timeout_seconds: int):
    assert context.containers["grafana-loki-server"].are_lines_present(
        lines, timeout_seconds, ssl=False
    ) or log_due_to_failure(context)


@then(
    '"{lines}" lines are published to the "{tenant_id}" tenant on the Grafana Loki server in less than {timeout_seconds:d} seconds'
)
@then(
    '"{lines}" line is published to the "{tenant_id}" tenant on the Grafana Loki server in less than {timeout_seconds:d} seconds'
)
def verify_lines_published_to_loki_tenant(context, lines: str, timeout_seconds: int, tenant_id: str):
    assert context.containers["grafana-loki-server"].are_lines_present(
        lines, timeout_seconds, ssl=False, tenant_id=tenant_id
    ) or log_due_to_failure(context)


@then('"{lines}" lines are published using SSL to the Grafana Loki server in less than {timeout_seconds:d} seconds')
@then('"{lines}" line is published using SSL to the Grafana Loki server in less than {timeout_seconds:d} seconds')
def verify_lines_published_to_loki_ssl(context, lines: str, timeout_seconds: int):
    assert context.containers["grafana-loki-server"].are_lines_present(
        lines, timeout_seconds, ssl=True
    ) or log_due_to_failure(context)


# Nginx reverse proxy
@step("a reverse proxy is set up to forward requests to the Grafana Loki server")
def setup_reverse_proxy_for_loki(context):
    context.containers["reverse-proxy"] = ReverseProxyContainer(context)
