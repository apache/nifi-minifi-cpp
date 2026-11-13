#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

from behave import given, step
from minifi_behave.containers.minifi_protocol import (
    conf_c2_flow_url,
    enable_log_metrics_publisher,
    enable_openssl_fips_mode,
    set_up_ssl_properties,
)
from minifi_behave.core.minifi_test_context import MinifiTestContext


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
    conf_c2_flow_url(context.get_or_create_default_minifi_container(), context.scenario_id)


@given("SSL properties are set in MiNiFi")
def set_minifi_ssl_properties(context: MinifiTestContext):
    set_up_ssl_properties(context.get_or_create_default_minifi_container())
