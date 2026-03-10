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
from minifi_test_framework.steps.flow_building_steps import add_ssl_context_service_for_minifi
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.helpers import log_due_to_failure
from minifi_test_framework.minifi.controller_service import ControllerService
from containers.couchbase_server_container import CouchbaseServerContainer


@step("a Couchbase server is started")
def step_impl(context: MinifiTestContext):
    context.containers["couchbase-server"] = CouchbaseServerContainer(context)
    assert context.containers["couchbase-server"].deploy()


@step("a CouchbaseClusterService controller service is set up to communicate with the Couchbase server")
def step_impl(context: MinifiTestContext):
    controller_service = ControllerService(class_name="CouchbaseClusterService", service_name="CouchbaseClusterService")
    controller_service.add_property("Connection String", f"couchbase://couchbase-server-{context.scenario_id}")
    controller_service.add_property("User Name", "Administrator")
    controller_service.add_property("User Password", "password123")
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(controller_service)


@step("a CouchbaseClusterService is set up using SSL connection")
def step_impl(context):
    ssl_context_service = ControllerService(class_name="SSLContextService", service_name="SSLContextService")
    ssl_context_service.add_property("CA Certificate", "/tmp/resources/root_ca.crt")
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(ssl_context_service)
    couchbase_cluster_service = ControllerService(class_name="CouchbaseClusterService", service_name="CouchbaseClusterService")
    couchbase_cluster_service.add_property("Connection String", f"couchbases://couchbase-server-{context.scenario_id}")
    couchbase_cluster_service.add_property("User Name", "Administrator")
    couchbase_cluster_service.add_property("User Password", "password123")
    couchbase_cluster_service.add_property("Linked Services", "SSLContextService")
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(couchbase_cluster_service)


@step("a CouchbaseClusterService is setup up using mTLS authentication")
def step_impl(context: MinifiTestContext):
    add_ssl_context_service_for_minifi(context, "clientuser")
    controller_service = ControllerService(class_name="CouchbaseClusterService", service_name="CouchbaseClusterService")
    controller_service.add_property("Connection String", f"couchbases://couchbase-server-{context.scenario_id}")
    controller_service.add_property("Linked Services", "SSLContextService")
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(controller_service)


@then("a document with id \"{doc_id}\" in bucket \"{bucket_name}\" is present with data '{data}' of type \"{data_type}\" in Couchbase")
def step_impl(context, doc_id: str, bucket_name: str, data: str, data_type: str):
    assert context.containers["couchbase-server"].is_data_present_in_couchbase(doc_id, bucket_name, data, data_type) or log_due_to_failure(context)
