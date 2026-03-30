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

from behave import given, when, then
from minifi_test_framework.core.helpers import retry_check
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


@given('controller socket properties are set up')
def setup_controller_socket_properties(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().set_controller_socket_properties()


@when('MiNiFi config is updated through MiNiFi controller')
def update_minifi_config_via_controller(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().update_flow_config_through_controller()


@then('the updated config is persisted')
def verify_updated_config_is_persisted(context: MinifiTestContext):
    assert context.get_or_create_default_minifi_container().updated_config_is_persisted()


@when('the {component} component is stopped through MiNiFi controller')
def stop_component_via_controller(context: MinifiTestContext, component: str):
    context.get_or_create_default_minifi_container().stop_component_through_controller(component)


@when('the {component} component is started through MiNiFi controller')
def start_component_via_controller(context: MinifiTestContext, component: str):
    context.get_or_create_default_minifi_container().start_component_through_controller(component)


@then('the {component} component is not running')
def verify_component_not_running(context: MinifiTestContext, component: str):
    assert not context.get_or_create_default_minifi_container().is_component_running(component)


@then('the {component} component is running')
def verify_component_is_running(context: MinifiTestContext, component: str):
    assert context.get_or_create_default_minifi_container().is_component_running(component)


@then('connection \"{connection}\" can be seen through MiNiFi controller')
def verify_connection_seen_via_controller(context: MinifiTestContext, connection: str):
    assert context.get_or_create_default_minifi_container().connection_found_through_controller(connection)


@then('{connection_count:d} connections can be seen full through MiNiFi controller')
def verify_full_connection_count(context: MinifiTestContext, connection_count: int):
    assert context.get_or_create_default_minifi_container().get_full_connection_count() == connection_count


@retry_check(10, 1)
def check_connection_size_through_controller(context: MinifiTestContext, connection: str, size: int, max_size: int) -> bool:
    return context.get_or_create_default_minifi_container().get_connection_size(connection) == (size, max_size)


@then('connection \"{connection}\" has {size:d} size and {max_size:d} max size through MiNiFi controller')
def verify_connection_size_via_controller(context: MinifiTestContext, connection: str, size: int, max_size: int):
    assert check_connection_size_through_controller(context, connection, size, max_size)


@retry_check(10, 1)
def manifest_can_be_retrieved_through_minifi_controller(context: MinifiTestContext) -> bool:
    manifest = context.get_or_create_default_minifi_container().get_manifest()
    return '"agentManifest": {' in manifest and '"componentManifest": {' in manifest and '"agentType": "cpp"' in manifest


@then('manifest can be retrieved through MiNiFi controller')
def verify_manifest_retrieval_via_controller(context: MinifiTestContext):
    assert manifest_can_be_retrieved_through_minifi_controller(context)


@then('debug bundle can be retrieved through MiNiFi controller')
def verify_debug_bundle_retrieval_via_controller(context: MinifiTestContext):
    assert context.get_or_create_default_minifi_container().create_debug_bundle()
