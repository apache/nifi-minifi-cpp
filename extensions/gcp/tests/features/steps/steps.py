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
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.helpers import log_due_to_failure
from containers.fake_gcs_server_container import FakeGcsServerContainer


@step("a Google Cloud storage server is set up")
@step("a Google Cloud storage server is set up with some test data")
@step('a Google Cloud storage server is set up and a single object with contents "preloaded data" is present')
def step_impl(context: MinifiTestContext):
    context.containers["fake-gcs-server"] = FakeGcsServerContainer(context)
    assert context.containers["fake-gcs-server"].deploy()


@then('an object with the content \"{content}\" is present in the Google Cloud storage')
def step_imp(context: MinifiTestContext, content: str):
    assert context.containers["fake-gcs-server"].check_google_cloud_storage(content) or log_due_to_failure(context)


@then("the test bucket of Google Cloud Storage is empty")
def step_impl(context: MinifiTestContext):
    assert context.containers["fake-gcs-server"].is_gcs_bucket_empty() or log_due_to_failure(context)
