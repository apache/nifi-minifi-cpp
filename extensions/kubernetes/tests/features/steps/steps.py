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

from behave import given

from minifi_test_framework.steps import checking_steps  # noqa: F401
from minifi_test_framework.steps import configuration_steps  # noqa: F401
from minifi_test_framework.steps import core_steps  # noqa: F401
from minifi_test_framework.steps import flow_building_steps  # noqa: F401

from minifi_test_framework.core.minifi_test_context import DEFAULT_MINIFI_CONTAINER_NAME, MinifiTestContext
from minifi_test_framework.minifi.processor import Processor
from minifi_test_framework.minifi.controller_service import ControllerService
from minifi_as_pod_in_kubernetes_cluster import MinifiAsPodInKubernetesCluster


def __ensure_kubernetes_cluster(context: MinifiTestContext):
    if DEFAULT_MINIFI_CONTAINER_NAME not in context.containers or not isinstance(context.containers[DEFAULT_MINIFI_CONTAINER_NAME], MinifiAsPodInKubernetesCluster):
        context.containers[DEFAULT_MINIFI_CONTAINER_NAME] = MinifiAsPodInKubernetesCluster("kubernetes", context)


@given("a {processor_type} processor in a Kubernetes cluster")
@given("a {processor_type} processor in the Kubernetes cluster")
def step_impl(context: MinifiTestContext, processor_type: str):
    __ensure_kubernetes_cluster(context)
    processor = Processor(class_name=processor_type, proc_name=processor_type)
    context.get_or_create_default_minifi_container().flow_definition.add_processor(processor)


def __set_up_the_kubernetes_controller_service(context: MinifiTestContext, processor_name: str, service_property_name: str, properties: dict[str, str]):
    kubernetes_controller_service = ControllerService(class_name="KubernetesControllerService", service_name="Kubernetes Controller Service")
    kubernetes_controller_service.properties = properties
    flow = context.get_or_create_default_minifi_container().flow_definition
    flow.controller_services.append(kubernetes_controller_service)
    flow.get_processor(processor_name).add_property(service_property_name, kubernetes_controller_service.name)


@given("the {processor_name} processor has a {service_property_name} which is a Kubernetes Controller Service")
@given("the {processor_name} processor has an {service_property_name} which is a Kubernetes Controller Service")
def step_impl(context: MinifiTestContext, processor_name: str, service_property_name: str):
    __set_up_the_kubernetes_controller_service(context, processor_name, service_property_name, {})


@given("the {processor_name} processor has a {service_property_name} which is a Kubernetes Controller Service with the \"{property_name}\" property set to \"{property_value}\"")
@given("the {processor_name} processor has an {service_property_name} which is a Kubernetes Controller Service with the \"{property_name}\" property set to \"{property_value}\"")
def step_impl(context: MinifiTestContext, processor_name: str, service_property_name: str, property_name: str, property_value: str):
    __set_up_the_kubernetes_controller_service(context, processor_name, service_property_name, {property_name: property_value})
