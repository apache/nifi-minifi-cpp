# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
from behave import given, step, then
from containers.prometheus_container import PrometheusContainer
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.steps import (
    checking_steps,  # noqa: F401
    configuration_steps,  # noqa: F401
    core_steps,  # noqa: F401
    flow_building_steps,  # noqa: F401
)


@step("a Prometheus server is set up")
def setup_prometheus_server(context: MinifiTestContext):
    context.containers["prometheus"] = PrometheusContainer(context)


@step("a Prometheus server is set up with SSL")
def setup_prometheus_server_with_ssl(context: MinifiTestContext):
    context.containers["prometheus"] = PrometheusContainer(context, ssl=True)


@then('"{metric_class}" are published to the Prometheus server in less than {timeout_seconds:d} seconds')
@then('"{metric_class}" is published to the Prometheus server in less than {timeout_seconds:d} seconds')
def verify_metric_class_published_to_prometheus(context: MinifiTestContext, metric_class: str, timeout_seconds: int):
    assert wait_for_condition(
        condition=lambda: context.containers["prometheus"].check_metric_class_on_prometheus(metric_class),
        timeout_seconds=timeout_seconds,
        bail_condition=lambda: context.containers["prometheus"].exited,
        context=context,
    )


@then(
    '"{metric_class}" processor metric is published to the Prometheus server in less than {timeout_seconds:d} seconds for "{processor_name}" processor'
)
def verify_processor_metric_published_to_prometheus(
    context: MinifiTestContext,
    metric_class: str,
    timeout_seconds: int,
    processor_name: str,
):
    assert wait_for_condition(
        condition=lambda: context.containers["prometheus"].check_processor_metric_on_prometheus(
            metric_class, processor_name
        ),
        timeout_seconds=timeout_seconds,
        bail_condition=lambda: context.containers["prometheus"].exited,
        context=context,
    )


@then("all Prometheus metric types are only defined once")
def verify_all_prometheus_metric_types_defined_once(context: MinifiTestContext):
    assert context.containers["prometheus"].check_all_metric_types_defined_once()


def _enable_prometheus(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().set_property("nifi.metrics.publisher.agent.identifier", "Agent1")
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.PrometheusMetricsPublisher.port", "9936"
    )
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.PrometheusMetricsPublisher.metrics",
        "RepositoryMetrics,QueueMetrics,PutFileMetrics,processorMetrics/Get.*,FlowInformation,DeviceInfoNode,AgentStatus",
    )
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.class", "PrometheusMetricsPublisher"
    )


@given("Prometheus is enabled in MiNiFi")
def enable_prometheus_in_minifi(context: MinifiTestContext):
    _enable_prometheus(context)


@given("Prometheus with SSL is enabled in MiNiFi")
def enable_prometheus_with_ssl_in_minifi(context: MinifiTestContext):
    _enable_prometheus(context)
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.PrometheusMetricsPublisher.certificate",
        "/tmp/resources/minifi_merged_cert.crt",
    )
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.PrometheusMetricsPublisher.ca.certificate",
        "/tmp/resources/root_ca.crt",
    )
