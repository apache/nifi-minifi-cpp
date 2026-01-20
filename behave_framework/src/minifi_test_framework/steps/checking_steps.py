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
import re

import humanfriendly
from behave import then, step

from minifi_test_framework.containers.http_proxy_container import HttpProxy
from minifi_test_framework.core.helpers import wait_for_condition, check_condition_after_wait
from minifi_test_framework.core.minifi_test_context import DEFAULT_MINIFI_CONTAINER_NAME, MinifiTestContext


@then('a file with the content "{content}" is placed on the path "{path}" in less than {duration}')
def step_impl(context: MinifiTestContext, content: str, path: str, duration: str):
    new_content = content.replace("\\n", "\n")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.get_default_minifi_container().path_with_content_exists(path, new_content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_default_minifi_container().exited, context=context)


@then('a single file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    new_content = content.replace("\\n", "\n")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.get_default_minifi_container().directory_has_single_file_with_content(directory, new_content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_default_minifi_container().exited, context=context)


@then('in the "{container_name}" container at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, container_name: str, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.get_minifi_container(container_name).directory_contains_file_with_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_minifi_container(container_name).exited, context=context)


@then('at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    context.execute_steps(f'then in the "{DEFAULT_MINIFI_CONTAINER_NAME}" container at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')


@then('the Minifi logs do not contain the following message: "{message}" after {duration}')
def step_impl(context: MinifiTestContext, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    time.sleep(duration_seconds)
    assert message not in context.get_default_minifi_container().get_logs()


@then("the Minifi logs do not contain errors")
def step_impl(context: MinifiTestContext):
    assert "[error]" not in context.get_default_minifi_container().get_logs() or context.get_default_minifi_container().log_app_output()


@then("the Minifi logs do not contain warnings")
def step_impl(context: MinifiTestContext):
    assert "[warning]" not in context.get_default_minifi_container().get_logs() or context.get_default_minifi_container().log_app_output()


@then("the Minifi logs contain the following message: '{message}' in less than {duration}")
@then('the Minifi logs contain the following message: "{message}" in less than {duration}')
def step_impl(context: MinifiTestContext, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: message in context.get_default_minifi_container().get_logs(),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.get_default_minifi_container().exited,
                              context=context)


@then("the Minifi logs contain the following message: \"{log_message}\" {count:d} times after {duration}")
def step_impl(context, log_message, count, duration):
    duration_seconds = humanfriendly.parse_timespan(duration)
    time.sleep(duration_seconds)
    assert context.get_default_minifi_container().get_logs().count(log_message) == count or context.get_default_minifi_container().log_app_output()


@then("the Minifi logs match the following regex: \"{regex}\" in less than {duration}")
def step_impl(context, regex, duration):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: re.search(regex, context.get_default_minifi_container().get_logs()),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.get_default_minifi_container().exited,
                              context=context)


@step('no errors were generated on the http-proxy regarding "{url}"')
def step_impl(context: MinifiTestContext, url: str):
    http_proxy_container = next(container for container in context.containers.values() if isinstance(container, HttpProxy))
    assert http_proxy_container.check_http_proxy_access(url) or http_proxy_container.log_app_output()


@then('in the "{container}" container no files are placed in the "{directory}" directory in {duration} of running time')
def step_impl(context, container, directory, duration):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert check_condition_after_wait(condition=lambda: context.get_minifi_container(container).get_number_of_files(directory) == 0,
                                      context=context, wait_time=duration_seconds)


@then('no files are placed in the "{directory}" directory in {duration} of running time')
def step_impl(context, directory, duration):
    context.execute_steps(f'then in the "{DEFAULT_MINIFI_CONTAINER_NAME}" container no files are placed in the "{directory}" directory in {duration} of running time')


@then('{num:d} files are placed in the "{directory}" directory in less than {duration}')
@then('{num:d} file is placed in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, num: int, directory: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    if num == 0:
        context.execute_steps(f'then no files are placed in the "{directory}" directory in {duration} of running time')
        return
    assert wait_for_condition(condition=lambda: context.get_default_minifi_container().get_number_of_files(directory) == num,
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.get_default_minifi_container().exited,
                              context=context)


@then('at least {num:d} files are placed in the "{directory}" directory in less than {duration}')
@then('at least {num:d} file is placed in the "{directory}" directory in less than {duration}')
def step_impl(context: MinifiTestContext, num: int, directory: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: context.get_default_minifi_container().get_number_of_files(directory) >= num,
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.get_default_minifi_container().exited,
                              context=context)


@then('at least one file in "{directory}" content match the following regex: "{regex_str}" in less than {duration}')
def step_impl(context: MinifiTestContext, directory: str, regex_str: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.get_default_minifi_container().directory_contains_file_with_regex(directory, regex_str),
        timeout_seconds=duration_seconds, bail_condition=lambda: context.get_default_minifi_container().exited, context=context)


@then('files with contents "{content_one}" and "{content_two}" are placed in the "{directory}" directory in less than {timeout}')
def step_impl(context: MinifiTestContext, directory: str, timeout: str, content_one: str, content_two: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout)
    c1 = content_one.replace("\\n", "\n")
    c2 = content_two.replace("\\n", "\n")
    contents_arr = [c1, c2]
    assert wait_for_condition(condition=lambda: context.get_default_minifi_container().verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_seconds, bail_condition=lambda: context.get_default_minifi_container().exited,
                              context=context)


@then('files with contents "{contents}" are placed in the "{directory}" directory in less than {timeout}')
def step_impl(context: MinifiTestContext, directory: str, timeout: str, contents: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout)
    new_contents = contents.replace("\\n", "\n")
    contents_arr = new_contents.split(",")
    assert wait_for_condition(condition=lambda: context.get_default_minifi_container().verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_seconds, bail_condition=lambda: context.get_default_minifi_container().exited,
                              context=context)


@then("a file with the JSON content \"{content}\" is placed in the \"{directory}\" directory in less than {duration}")
@then("a file with the JSON content '{content}' is placed in the '{directory}' directory in less than {duration}")
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.get_default_minifi_container().verify_path_with_json_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_default_minifi_container().exited, context=context)


@then('MiNiFi\'s memory usage does not increase by more than {max_increase} after {duration}')
def step_impl(context: MinifiTestContext, max_increase: str, duration: str):
    time_in_seconds = humanfriendly.parse_timespan(duration)
    max_increase_in_bytes = humanfriendly.parse_size(max_increase)
    initial_memory_usage = context.get_default_minifi_container().get_memory_usage()
    time.sleep(time_in_seconds)
    final_memory_usage = context.get_default_minifi_container().get_memory_usage()
    assert final_memory_usage - initial_memory_usage <= max_increase_in_bytes


@then("at least one file with the JSON content \"{content}\" is placed in the \"{directory}\" directory in less than {duration}")
@then("at least one file with the JSON content '{content}' is placed in the '{directory}' directory in less than {duration}")
def step_impl(context: MinifiTestContext, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.get_default_minifi_container().directory_contains_file_with_json_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_default_minifi_container().exited, context=context)


@then('after a wait of {duration}, at least {lower_bound:d} and at most {upper_bound:d} files are produced and placed in the "{directory}" directory')
def step_impl(context: MinifiTestContext, lower_bound: int, upper_bound: int, duration: str, directory: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert check_condition_after_wait(condition=lambda: context.get_default_minifi_container().get_number_of_files(directory) >= lower_bound
                                      and context.get_default_minifi_container().get_number_of_files(directory) <= upper_bound,
                                      context=context, wait_time=duration_seconds)


@then('exactly these files are in the "{directory}" directory in less than {duration}: "{contents}"')
def step_impl(context: MinifiTestContext, directory: str, duration: str, contents: str):
    if not contents:
        context.execute_steps(f'then no files are placed in the "{directory}" directory in {duration} of running time')
        return
    contents_arr = contents.split(",")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: context.get_default_minifi_container().verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_in_seconds, bail_condition=lambda: False,
                              context=context)


@then('exactly these files are in the "{directory}" directory in less than {duration}: ""')
def step_impl(context, directory, duration):
    context.execute_steps(f'then no files are placed in the "{directory}" directory in {duration} of running time')


@then("at least one empty file is placed in the \"{directory}\" directory in less than {duration}")
def step_impl(context: MinifiTestContext, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.get_default_minifi_container().directory_contains_empty_file(directory),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_default_minifi_container().exited, context=context)


@then("in the \"{container_name}\" container at least one empty file is placed in the \"{directory}\" directory in less than {duration}")
def step_impl(context: MinifiTestContext, container_name: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    if container_name == "nifi":
        assert wait_for_condition(
            condition=lambda: context.containers["nifi"].directory_contains_empty_file(directory),
            timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_minifi_container(container_name).exited, context=context)
        return
    assert wait_for_condition(
        condition=lambda: context.get_minifi_container(container_name).directory_contains_empty_file(directory),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_minifi_container(container_name).exited, context=context)


@then("in the \"{container_name}\" container at least one file with minimum size of \"{size}\" is placed in the \"{directory}\" directory in less than {duration}")
def step_impl(context: MinifiTestContext, container_name: str, directory: str, size: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    size_in_bytes = humanfriendly.parse_size(size)
    assert wait_for_condition(
        condition=lambda: context.get_minifi_container(container_name).directory_contains_file_with_minimum_size(directory, size_in_bytes),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.get_minifi_container(container_name).exited, context=context)


@then("at least one file with minimum size of \"{size}\" is placed in the \"{directory}\" directory in less than {duration}")
def step_impl(context: MinifiTestContext, directory: str, size: str, duration: str):
    context.execute_steps(
        f'Then in the "{DEFAULT_MINIFI_CONTAINER_NAME}" container at least one file with minimum size of "{size}" is placed in the "{directory}" directory in less than {duration}')


@then("the MiNiFi C2 server logs contain the following message: \"{log_message}\" in less than {duration}")
def step_impl(context: MinifiTestContext, log_message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: log_message in context.get_minifi_container("minifi-c2-server").get_logs(),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.get_minifi_container("minifi-c2-server").exited,
                              context=context)
