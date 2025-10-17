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

from behave import step, given

from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.containers.minifi_protocol import enable_openssl_fips_mode
from minifi_test_framework.containers.minifi_protocol import set_up_ssl_properties
from minifi_test_framework.containers.minifi_protocol import enable_log_metrics_publisher
from minifi_test_framework.containers.minifi_protocol import configure_c2_flow_url


@step('MiNiFi configuration "{config_key}" is set to "{config_value}"')
def set_minifi_config_property(context: MinifiTestContext, config_key: str, config_value: str):
    context.get_or_create_default_minifi_container().set_property(config_key, config_value)


@step("log metrics publisher is enabled in MiNiFi")
def enable_minifi_log_metrics_publisher(context: MinifiTestContext):
    enable_log_metrics_publisher(context.get_or_create_default_minifi_container())


@step('log property "{log_property_key}" is set to "{log_property_value}"')
def set_minifi_log_property(context: MinifiTestContext, log_property_key: str, log_property_value: str):
    context.get_or_create_default_minifi_container().set_log_property(log_property_key, log_property_value)


@given("OpenSSL FIPS mode is enabled in MiNiFi")
def enable_minifi_openssl_fips_mode(context: MinifiTestContext):
    enable_openssl_fips_mode(context.get_or_create_default_minifi_container())


@given("the C2 flow URL property is configured")
def configure_c2_flow_url(context: MinifiTestContext):
    configure_c2_flow_url(context.get_or_create_default_minifi_container(), context.scenario_id)


@given("SSL properties are set in MiNiFi")
def set_minifi_ssl_properties(context: MinifiTestContext):
    set_up_ssl_properties(context.get_or_create_default_minifi_container())
