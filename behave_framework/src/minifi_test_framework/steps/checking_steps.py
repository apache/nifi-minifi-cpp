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


import time

import humanfriendly
from behave import then, step

from minifi_test_framework.containers.http_proxy_container import HttpProxy
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


@then('there is a file with "{content}" content at {path} in less than {duration}')
def step_impl(context: MinifiTestContext, content: str, path: str, duration: str):
    new_content = content.replace("\\n", "\n")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.minifi_container.path_with_content_exists(path, new_content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.minifi_container.exited, context=context)


@then('there is a single file with "{content}" content in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    new_content = content.replace("\\n", "\n")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.minifi_container.directory_has_single_file_with_content(directory, new_content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.minifi_container.exited, context=context)


@then('at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.minifi_container.directory_contains_file_with_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.minifi_container.exited, context=context)


@then('the Minifi logs do not contain the following message: "{message}" after {duration}')
def step_impl(context: MinifiTestContext, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    time.sleep(duration_seconds)
    assert message not in context.minifi_container.get_logs()


@then("the Minifi logs do not contain errors")
def step_impl(context: MinifiTestContext):
    assert "[error]" not in context.minifi_container.get_logs() or context.minifi_container.log_app_output()


@then("the Minifi logs do not contain warnings")
def step_impl(context: MinifiTestContext):
    assert "[warning]" not in context.minifi_container.get_logs() or context.minifi_container.log_app_output()


@then("the Minifi logs contain the following message: '{message}' in less than {duration}")
@then('the Minifi logs contain the following message: "{message}" in less than {duration}')
def step_impl(context: MinifiTestContext, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: message in context.minifi_container.get_logs(),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.minifi_container.exited,
                              context=context)


@step('no errors were generated on the http-proxy regarding "{url}"')
def step_impl(context: MinifiTestContext, url: str):
    http_proxy_container = next(container for container in context.containers if isinstance(container, HttpProxy))
    assert http_proxy_container.check_http_proxy_access(url) or http_proxy_container.log_app_output()


@then('there are {num_str} files in the "{directory}" directory in less than {duration}')
@then('there is {num_str} file in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, num_str: str, directory: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: context.minifi_container.get_number_of_files(directory) == int(num_str),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.minifi_container.exited,
                              context=context)


@then('there are at least {num_str} files is in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, num_str: str, directory: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: context.minifi_container.get_number_of_files(directory) >= int(num_str),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.minifi_container.exited,
                              context=context)


@then('at least one file in "{directory}" content match the following regex: "{regex_str}" in less than {duration}')
def step_impl(context: MinifiTestContext, directory: str, regex_str: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.minifi_container.directory_contains_file_with_regex(directory, regex_str),
        timeout_seconds=duration_seconds, bail_condition=lambda: context.minifi_container.exited, context=context)


@then('the contents of {directory} in less than {timeout} are: "{content_one}" and "{content_two}"')
def step_impl(context: MinifiTestContext, directory: str, timeout: str, content_one: str, content_two: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout)
    c1 = content_one.replace("\\n", "\n")
    c2 = content_two.replace("\\n", "\n")
    contents_arr = [c1, c2]
    assert wait_for_condition(condition=lambda: context.minifi_container.verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_seconds, bail_condition=lambda: context.minifi_container.exited,
                              context=context)


@then('the contents of {directory} in less than {timeout} are: "{contents}"')
def step_impl(context: MinifiTestContext, directory: str, timeout: str, contents: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout)
    new_contents = contents.replace("\\n", "\n")
    contents_arr = new_contents.split(",")
    assert wait_for_condition(condition=lambda: context.minifi_container.verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_seconds, bail_condition=lambda: context.minifi_container.exited,
                              context=context)


@then("a flowfile with the JSON content \"{content}\" is placed in {directory} in less than {duration}")
@then("a flowfile with the JSON content '{content}' is placed in {directory} in less than {duration}")
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.minifi_container.verify_path_with_json_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.minifi_container.exited, context=context)
