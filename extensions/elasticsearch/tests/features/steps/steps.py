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
from behave import step, given, then

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.minifi.controller_service import ControllerService
from minifi_test_framework.core.helpers import log_due_to_failure
from containers.elasticsearch_container import ElasticsearchContainer
from containers.opensearch_container import OpensearchContainer


@step('an Elasticsearch server is set up and running')
@step('an Elasticsearch server is set up and a single document is present with "preloaded_id" in "my_index"')
@step('an Elasticsearch server is set up and a single document is present with "preloaded_id" in "my_index" with "value1" in "field1"')
def step_impl(context: MinifiTestContext):
    context.containers["elasticsearch"] = ElasticsearchContainer(context)
    assert context.containers["elasticsearch"].deploy()
    assert context.containers["elasticsearch"].create_doc_elasticsearch("my_index", "preloaded_id") or context.containers["elasticsearch"].log_app_output()


@given('an ElasticsearchCredentialsControllerService is set up with Basic Authentication')
def step_impl(context: MinifiTestContext):
    controller_service = ControllerService(class_name="ElasticsearchCredentialsControllerService", service_name="ElasticsearchCredentialsControllerService")
    controller_service.add_property("Username", "elastic")
    controller_service.add_property("Password", "password")
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(controller_service)


@given('an ElasticsearchCredentialsControllerService is set up with ApiKey')
def step_impl(context: MinifiTestContext):
    controller_service = ControllerService(class_name="ElasticsearchCredentialsControllerService", service_name="ElasticsearchCredentialsControllerService")
    api_key = context.containers["elasticsearch"].elastic_generate_apikey()
    controller_service.add_property("API Key", api_key)
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(controller_service)


@then('Elasticsearch has a document with "{doc_id}" in "{index}" that has "{value}" set in "{field}"')
def step_impl(context: MinifiTestContext, doc_id: str, index: str, value: str, field: str):
    assert context.containers["elasticsearch"].check_elastic_field_value(index_name=index, doc_id=doc_id, field_name=field, field_value=value) or log_due_to_failure(context)


@then("Elasticsearch is empty")
def step_impl(context):
    assert context.containers["elasticsearch"].is_elasticsearch_empty() or log_due_to_failure(context)


@given('an Opensearch server is set up and running')
@given('an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index"')
@given('an Opensearch server is set up and a single document is present with "preloaded_id" in "my_index" with "value1" in "field1"')
def step_impl(context):
    context.containers["opensearch"] = OpensearchContainer(context)
    context.containers["opensearch"].deploy()
    context.containers["opensearch"].add_elastic_user_to_opensearch()
    context.containers["opensearch"].create_doc_elasticsearch("my_index", "preloaded_id")


@then('Opensearch has a document with "{doc_id}" in "{index}" that has "{value}" set in "{field}"')
def step_impl(context: MinifiTestContext, doc_id: str, index: str, value: str, field: str):
    assert context.containers["opensearch"].check_elastic_field_value(index_name=index, doc_id=doc_id, field_name=field, field_value=value) or log_due_to_failure(context)


@then("Opensearch is empty")
def step_impl(context):
    assert context.containers["opensearch"].is_elasticsearch_empty() or log_due_to_failure(context)
