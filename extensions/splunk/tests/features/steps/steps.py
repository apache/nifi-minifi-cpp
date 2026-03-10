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
from minifi_test_framework.core.helpers import log_due_to_failure
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from containers.splunk_container import SplunkContainer


@step("a Splunk HEC is set up and running")
def step_impl(context: MinifiTestContext):
    context.containers["splunk"] = SplunkContainer(context)
    assert context.containers["splunk"].deploy()
    assert context.containers["splunk"].enable_splunk_hec_indexer('splunk_hec_token') or context.containers["splunk"].log_app_output()


@then('an event is registered in Splunk HEC with the content \"{content}\"')
def step_imp(context, content):
    assert context.containers["splunk"].check_splunk_event(content) or log_due_to_failure(context)


@then('an event is registered in Splunk HEC with the content \"{content}\" with \"{source}\" set as source and \"{source_type}\" set as sourcetype and \"{host}\" set as host')
def step_imp(context, content, source, source_type, host):
    attr = {"source": source, "sourcetype": source_type, "host": host}
    assert context.containers["splunk"].check_splunk_event_with_attributes(content, attr) or log_due_to_failure(context)


@step("SSL is enabled for the Splunk HEC and the SSL context service is set up for PutSplunkHTTP and QuerySplunkIndexingStatus")
def step_impl(context):
    assert context.containers["splunk"].enable_splunk_hec_ssl() or context.containers["splunk"].log_app_output()
