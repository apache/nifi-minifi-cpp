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
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from prometheus_container import PrometheusContainer


@step('a Prometheus server is set up')
def step_impl(context: MinifiTestContext):
    context.containers["prometheus"] = PrometheusContainer(context)


@step("a Prometheus server is set up with SSL")
def step_impl(context: MinifiTestContext):
    context.containers["prometheus"] = PrometheusContainer(context, ssl=True)


@then("\"{metric_class}\" are published to the Prometheus server in less than {timeout_seconds:d} seconds")
@then("\"{metric_class}\" is published to the Prometheus server in less than {timeout_seconds:d} seconds")
def step_impl(context: MinifiTestContext, metric_class: str, timeout_seconds: int):
    assert wait_for_condition(
        condition=lambda: context.containers["prometheus"].check_metric_class_on_prometheus(metric_class),
        timeout_seconds=timeout_seconds, bail_condition=lambda: context.containers["prometheus"].exited, context=context)


@then("\"{metric_class}\" processor metric is published to the Prometheus server in less than {timeout_seconds:d} seconds for \"{processor_name}\" processor")
def step_impl(context: MinifiTestContext, metric_class: str, timeout_seconds: int, processor_name: str):
    assert wait_for_condition(
        condition=lambda: context.containers["prometheus"].check_processor_metric_on_prometheus(metric_class, processor_name),
        timeout_seconds=timeout_seconds, bail_condition=lambda: context.containers["prometheus"].exited, context=context)


@then("all Prometheus metric types are only defined once")
def step_impl(context: MinifiTestContext):
    assert context.containers["prometheus"].check_all_metric_types_defined_once()
