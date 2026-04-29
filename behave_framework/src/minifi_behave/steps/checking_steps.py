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

from minifi_behave.containers.http_proxy_container import HttpProxy
from minifi_behave.core.helpers import wait_for_condition, check_condition_after_wait, log_due_to_failure
from minifi_behave.core.minifi_test_context import DEFAULT_MINIFI_CONTAINER_NAME, MinifiTestContext


@then('a file with the content "{content}" is placed on the path "{path}" in less than {duration}')
def verify_file_content_on_path(context: MinifiTestContext, content: str, path: str, duration: str):
    new_content = content.replace("\\n", "\n")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].path_with_content_exists(path, new_content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited, context=context)


@then('in the "{container_name}" container a single file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
def verify_single_file_content_in_container_directory(context: MinifiTestContext, container_name: str, content: str, directory: str, duration: str):
    new_content = content.replace("\\n", "\n")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[container_name].directory_has_single_file_with_content(directory, new_content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[container_name].exited, context=context)


@then('a single file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
@then("a single file with the content '{content}' is placed in the '{directory}' directory in less than {duration}")
def verify_single_file_content_in_directory(context: MinifiTestContext, content: str, directory: str, duration: str):
    context.execute_steps(f'then in the "{DEFAULT_MINIFI_CONTAINER_NAME}" container a single file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')


@then('in the "{container_name}" container at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
def verify_at_least_one_file_content_in_container_directory(context: MinifiTestContext, container_name: str, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[container_name].directory_contains_file_with_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[container_name].exited, context=context)


@then('at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')
@then("at least one file with the content '{content}' is placed in the '{directory}' directory in less than {duration}")
def verify_at_least_one_file_content_in_directory(context: MinifiTestContext, content: str, directory: str, duration: str):
    context.execute_steps(f'then in the "{DEFAULT_MINIFI_CONTAINER_NAME}" container at least one file with the content "{content}" is placed in the "{directory}" directory in less than {duration}')


@then("the logs of the '{container}' container do not contain the following message: '{message}' after {duration}")
@then('the logs of the "{container}" container do not contain the following message: "{message}" after {duration}')
def verify_container_logs_do_not_contain_message(context: MinifiTestContext, container: str, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    time.sleep(duration_seconds)
    assert message not in context.containers[container].get_logs() or log_due_to_failure(context)


@then('the Minifi logs do not contain the following message: "{message}" after {duration}')
def verify_minifi_logs_do_not_contain_message(context: MinifiTestContext, message: str, duration: str):
    context.execute_steps(f'then the logs of the "{DEFAULT_MINIFI_CONTAINER_NAME}" container do not contain the following message: "{message}" after {duration}')


@then("the Minifi logs do not contain errors")
def verify_minifi_logs_do_not_contain_errors(context: MinifiTestContext):
    assert "[error]" not in context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_logs() or log_due_to_failure(context)


@then("the Minifi logs do not contain warnings")
def verify_minifi_logs_do_not_contain_warnings(context: MinifiTestContext):
    assert "[warning]" not in context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_logs() or log_due_to_failure(context)


@then("the logs of the '{container}' container contain the following message: '{message}' in less than {duration}")
@then('the logs of the "{container}" container contain the following message: "{message}" in less than {duration}')
def verify_container_logs_contain_message(context: MinifiTestContext, container: str, message: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: message in context.containers[container].get_logs(),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.containers[container].exited,
                              context=context)


@then("the Minifi logs contain the following message: '{message}' in less than {duration}")
@then('the Minifi logs contain the following message: "{message}" in less than {duration}')
def verify_minifi_logs_contain_message(context: MinifiTestContext, message: str, duration: str):
    context.execute_steps(f'then the logs of the "{DEFAULT_MINIFI_CONTAINER_NAME}" container contain the following message: "{message}" in less than {duration}')


@then("the Minifi logs contain the following message: \"{log_message}\" {count:d} times after {duration}")
def verify_minifi_logs_contain_message_multiple_times(context, log_message, count, duration):
    duration_seconds = humanfriendly.parse_timespan(duration)
    time.sleep(duration_seconds)
    assert context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_logs().count(log_message) == count or log_due_to_failure(context)


@then("the Minifi logs match the following regex: \"{regex}\" in less than {duration}")
def verify_minifi_logs_match_regex(context, regex, duration):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: re.search(regex, context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_logs()),
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited,
                              context=context)


@step('no errors were generated on the http-proxy regarding "{url}"')
def verify_no_errors_on_http_proxy(context: MinifiTestContext, url: str):
    http_proxy_container = next(container for container in context.containers.values() if isinstance(container, HttpProxy))
    assert http_proxy_container.check_http_proxy_access(url) or http_proxy_container.log_app_output()


@then('in the "{container}" container no files are placed in the "{directory}" directory in {duration} of running time')
def verify_no_files_in_container_directory(context, container, directory, duration):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert check_condition_after_wait(condition=lambda: context.containers[container].get_number_of_files(directory) == 0,
                                      context=context, wait_time=duration_seconds)


@then('no files are placed in the "{directory}" directory in {duration} of running time')
def verify_no_files_in_directory(context, directory, duration):
    context.execute_steps(f'then in the "{DEFAULT_MINIFI_CONTAINER_NAME}" container no files are placed in the "{directory}" directory in {duration} of running time')


@then('{num:d} files are placed in the "{directory}" directory in less than {duration}')
@then('{num:d} file is placed in the "{directory}" directory in less than {duration}')
def verify_number_of_files_in_directory(context: MinifiTestContext, num: int, directory: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    if num == 0:
        context.execute_steps(f'then no files are placed in the "{directory}" directory in {duration} of running time')
        return
    assert wait_for_condition(condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_number_of_files(directory) == num,
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited,
                              context=context)


@then('at least {num:d} files are placed in the "{directory}" directory in less than {duration}')
@then('at least {num:d} file is placed in the "{directory}" directory in less than {duration}')
def verify_at_least_number_of_files_in_directory(context: MinifiTestContext, num: int, directory: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_number_of_files(directory) >= num,
                              timeout_seconds=duration_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited,
                              context=context)


@then('at least one file in "{directory}" content match the following regex: "{regex_str}" in less than {duration}')
@then('the content of at least one file in the "{directory}" directory matches the \'{regex_str}\' regex in less than {duration}')
def verify_file_content_matches_regex_in_directory(context: MinifiTestContext, directory: str, regex_str: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].directory_contains_file_with_regex(directory, regex_str),
        timeout_seconds=duration_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited, context=context)


@then('files with contents "{content_one}" and "{content_two}" are placed in the "{directory}" directory in less than {timeout}')
@then("files with contents '{content_one}' and '{content_two}' are placed in the '{directory}' directory in less than {timeout}")
def verify_files_with_two_contents_in_directory(context: MinifiTestContext, directory: str, timeout: str, content_one: str, content_two: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout)
    c1 = content_one.replace("\\n", "\n")
    c2 = content_two.replace("\\n", "\n")
    contents_arr = [c1, c2]
    assert wait_for_condition(condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited,
                              context=context)


@then('files with contents "{contents}" are placed in the "{directory}" directory in less than {timeout}')
def verify_files_with_contents_in_directory(context: MinifiTestContext, directory: str, timeout: str, contents: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout)
    new_contents = contents.replace("\\n", "\n")
    contents_arr = new_contents.split(",")
    assert wait_for_condition(condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited,
                              context=context)


@then('files with at least these contents "{contents}" are placed in the "{directory}" directory in less than {timeout}')
def verify_files_with_at_least_contents_in_directory(context: MinifiTestContext, directory: str, timeout: str, contents: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout)
    new_contents = contents.replace("\\n", "\n")
    contents_arr = new_contents.split(",")
    assert wait_for_condition(condition=lambda: all([context.containers[DEFAULT_MINIFI_CONTAINER_NAME].directory_contains_file_with_content(directory, content) for content in contents_arr]),
                              timeout_seconds=timeout_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited,
                              context=context)


@then("a file with the JSON content \"{content}\" is placed in the \"{directory}\" directory in less than {duration}")
@then("a file with the JSON content '{content}' is placed in the '{directory}' directory in less than {duration}")
def verify_file_with_json_content_in_directory(context: MinifiTestContext, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].verify_path_with_json_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited, context=context)


@then('MiNiFi\'s memory usage does not increase by more than {max_increase} after {duration}')
def verify_minifi_memory_usage_increase(context: MinifiTestContext, max_increase: str, duration: str):
    time_in_seconds = humanfriendly.parse_timespan(duration)
    max_increase_in_bytes = humanfriendly.parse_size(max_increase)
    initial_memory_usage = context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_memory_usage()
    time.sleep(time_in_seconds)
    final_memory_usage = context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_memory_usage()
    assert final_memory_usage - initial_memory_usage <= max_increase_in_bytes


@then("at least one file with the JSON content \"{content}\" is placed in the \"{directory}\" directory in less than {duration}")
@then("at least one file with the JSON content '{content}' is placed in the '{directory}' directory in less than {duration}")
def verify_at_least_one_file_with_json_content_in_directory(context: MinifiTestContext, content: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].directory_contains_file_with_json_content(directory, content),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited, context=context)


@then('after a wait of {duration}, at least {lower_bound:d} and at most {upper_bound:d} files are produced and placed in the "{directory}" directory')
def verify_file_count_bounds_in_directory(context: MinifiTestContext, lower_bound: int, upper_bound: int, duration: str, directory: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert check_condition_after_wait(condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_number_of_files(directory) >= lower_bound
                                      and context.containers[DEFAULT_MINIFI_CONTAINER_NAME].get_number_of_files(directory) <= upper_bound,
                                      context=context, wait_time=duration_seconds)


@then('exactly these files are in the "{directory}" directory in less than {duration}: "{contents}"')
def verify_exact_files_in_directory(context: MinifiTestContext, directory: str, duration: str, contents: str):
    if not contents:
        context.execute_steps(f'then no files are placed in the "{directory}" directory in {duration} of running time')
        return
    contents_arr = contents.split(",")
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].verify_file_contents(directory, contents_arr),
                              timeout_seconds=timeout_in_seconds, bail_condition=lambda: False,
                              context=context)


@then('exactly these files are in the "{directory}" directory in less than {duration}: ""')
def verify_no_files_in_directory(context, directory, duration):
    context.execute_steps(f'then no files are placed in the "{directory}" directory in {duration} of running time')


@then("at least one empty file is placed in the \"{directory}\" directory in less than {duration}")
def verify_at_least_one_empty_file_in_directory(context: MinifiTestContext, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].directory_contains_empty_file(directory),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[DEFAULT_MINIFI_CONTAINER_NAME].exited, context=context)


@then("in the \"{container_name}\" container at least one empty file is placed in the \"{directory}\" directory in less than {duration}")
def verify_at_least_one_empty_file_in_container_directory(context: MinifiTestContext, container_name: str, directory: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: context.containers[container_name].directory_contains_empty_file(directory),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[container_name].exited, context=context)


@then("in the \"{container_name}\" container at least one file with minimum size of \"{size}\" is placed in the \"{directory}\" directory in less than {duration}")
def verify_file_with_minimum_size_in_container_directory(context: MinifiTestContext, container_name: str, directory: str, size: str, duration: str):
    timeout_in_seconds = humanfriendly.parse_timespan(duration)
    size_in_bytes = humanfriendly.parse_size(size)
    assert wait_for_condition(
        condition=lambda: context.containers[container_name].directory_contains_file_with_minimum_size(directory, size_in_bytes),
        timeout_seconds=timeout_in_seconds, bail_condition=lambda: context.containers[container_name].exited, context=context)


@then("at least one file with minimum size of \"{size}\" is placed in the \"{directory}\" directory in less than {duration}")
def verify_file_with_minimum_size_in_directory(context: MinifiTestContext, directory: str, size: str, duration: str):
    context.execute_steps(
        f'Then in the "{DEFAULT_MINIFI_CONTAINER_NAME}" container at least one file with minimum size of "{size}" is placed in the "{directory}" directory in less than {duration}')
