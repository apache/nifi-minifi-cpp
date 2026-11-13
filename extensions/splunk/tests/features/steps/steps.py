# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
from behave import step, then
from containers.splunk_container import SplunkContainer
from minifi_behave.core.helpers import log_due_to_failure
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.steps import (
    checking_steps,  # noqa: F401
    configuration_steps,  # noqa: F401
    core_steps,  # noqa: F401
    flow_building_steps,  # noqa: F401
)


@step("a Splunk HEC is set up and running")
def setup_splunk_hec(context: MinifiTestContext):
    context.containers["splunk"] = SplunkContainer(context)
    assert context.containers["splunk"].deploy(context)
    assert (
        context.containers["splunk"].enable_splunk_hec_indexer("splunk_hec_token")
        or context.containers["splunk"].log_app_output()
    )


@then('an event is registered in Splunk HEC with the content "{content}"')
def verify_splunk_event_content(context, content):
    assert context.containers["splunk"].check_splunk_event(content) or log_due_to_failure(context)


@then(
    'an event is registered in Splunk HEC with the content "{content}" with "{source}" set as source and "{source_type}" set as sourcetype and "{host}" set as host'
)
def verify_splunk_event_with_attributes(context, content, source, source_type, host):
    attr = {"source": source, "sourcetype": source_type, "host": host}
    assert context.containers["splunk"].check_splunk_event_with_attributes(content, attr) or log_due_to_failure(context)


@step(
    "SSL is enabled for the Splunk HEC and the SSL context service is set up for PutSplunkHTTP and QuerySplunkIndexingStatus"
)
def enable_splunk_hec_ssl(context):
    assert context.containers["splunk"].enable_splunk_hec_ssl() or context.containers["splunk"].log_app_output()
