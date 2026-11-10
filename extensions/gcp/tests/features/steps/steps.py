# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
from behave import step, then
from containers.fake_gcs_server_container import FakeGcsServerContainer
from minifi_behave.core.helpers import log_due_to_failure
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.steps import (
    checking_steps,  # noqa: F401
    configuration_steps,  # noqa: F401
    core_steps,  # noqa: F401
    flow_building_steps,  # noqa: F401
)


@step("a Google Cloud storage server is set up")
@step("a Google Cloud storage server is set up with some test data")
@step('a Google Cloud storage server is set up and a single object with contents "preloaded data" is present')
def setup_gcs_server(context: MinifiTestContext):
    context.containers["fake-gcs-server"] = FakeGcsServerContainer(context)
    assert context.containers["fake-gcs-server"].deploy(context)


@then('an object with the content "{content}" is present in the Google Cloud storage')
def verify_gcs_object_content(context: MinifiTestContext, content: str):
    assert context.containers["fake-gcs-server"].check_google_cloud_storage(content) or log_due_to_failure(context)


@then("the test bucket of Google Cloud Storage is empty")
def verify_gcs_bucket_is_empty(context: MinifiTestContext):
    assert context.containers["fake-gcs-server"].is_gcs_bucket_empty() or log_due_to_failure(context)
