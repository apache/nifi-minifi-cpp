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

import logging
import random
import string
import os
import time
import uuid

import humanfriendly
from behave import when, step, given

from minifi_test_framework.containers.http_proxy_container import HttpProxy
from minifi_test_framework.containers.nifi_container import NifiContainer
from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.file import File
from minifi_test_framework.core.minifi_test_context import DEFAULT_MINIFI_CONTAINER_NAME, MinifiTestContext


@when("both instances start up")
@when("all instances start up")
def step_impl(context: MinifiTestContext):
    for container in context.containers.values():
        assert container.deploy() or container.log_app_output()
    logging.debug("All instances started up")


@when("the MiNiFi instance starts up")
def step_impl(context: MinifiTestContext):
    assert context.get_or_create_default_minifi_container().deploy()
    logging.debug("MiNiFi instance started up")


@step('a directory at "{directory}" has a file with the size "{size}"')
def step_impl(context: MinifiTestContext, directory: str, size: str):
    size = humanfriendly.parse_size(size)
    content = ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(size))
    dirs = context.get_or_create_default_minifi_container().dirs
    if directory in dirs:
        dirs[directory].files[str(uuid.uuid4())] = content
        return
    new_dir = Directory(directory)
    new_dir.files[str(uuid.uuid4())] = content
    dirs.append(new_dir)


def __add_directory_with_file_to_container(context: MinifiTestContext, directory: str, file_name: str, content: str, container_name: str):
    dirs = context.get_or_create_minifi_container(container_name).dirs
    new_content = content.replace("\\n", "\n")
    if directory in dirs:
        dirs[directory].files[file_name] = new_content
        return
    new_dir = Directory(directory)
    new_dir.files[file_name] = new_content
    dirs.append(new_dir)


@step('a directory at "{directory}" has a file with the content "{content}" in the "{flow_name}" flow')
@step("a directory at '{directory}' has a file with the content '{content}' in the '{flow_name}' flow")
def step_impl(context: MinifiTestContext, directory: str, content: str, flow_name: str):
    __add_directory_with_file_to_container(context, directory, str(uuid.uuid4()), content, flow_name)


@step('a directory at "{directory}" has a file with the content "{content}"')
@step("a directory at '{directory}' has a file with the content '{content}'")
def step_impl(context: MinifiTestContext, directory: str, content: str):
    context.execute_steps(f'given a directory at "{directory}" has a file with the content "{content}" in the "{DEFAULT_MINIFI_CONTAINER_NAME}" flow')


@step('a directory at "{directory}" has a file ("{file_name}") with the content "{content}"')
def step_impl(context: MinifiTestContext, directory: str, file_name: str, content: str):
    __add_directory_with_file_to_container(context, directory, file_name, content, DEFAULT_MINIFI_CONTAINER_NAME)


@step('a file with filename "{file_name}" and content "{content}" is present in "{path}"')
def step_impl(context: MinifiTestContext, file_name: str, content: str, path: str):
    new_content = content.replace("\\n", "\n")
    context.get_or_create_default_minifi_container().files.append(File(os.path.join(path, file_name), new_content))


@given('a file with the content "{content}" is present in "{path}" in the "{container_name}" flow')
def step_impl(context: MinifiTestContext, content: str, path: str, container_name: str):
    new_content = content.replace("\\n", "\n")
    context.get_or_create_minifi_container(container_name).files.append(File(os.path.join(path, str(uuid.uuid4())), new_content))


@given('a file with the content "{content}" is present in "{path}"')
@given("a file with the content '{content}' is present in '{path}'")
def step_impl(context: MinifiTestContext, content: str, path: str):
    context.execute_steps(f"given a file with the content \"{content}\" is present in \"{path}\" in the \"{DEFAULT_MINIFI_CONTAINER_NAME}\" flow")


@when('a file with the content "{content}" is placed in "{path}" in the "{container_name}" flow')
def step_impl(context: MinifiTestContext, content: str, path: str, container_name: str):
    new_content = content.replace("\\n", "\n")
    context.containers[container_name].add_file_to_running_container(new_content, path)


@when('a file with the content "{content}" is placed in "{path}"')
def step_impl(context: MinifiTestContext, content: str, path: str):
    context.execute_steps(f"when a file with the content \"{content}\" is placed in \"{path}\" in the \"{DEFAULT_MINIFI_CONTAINER_NAME}\" flow")


@given("an empty file is present in \"{path}\"")
def step_impl(context, path):
    context.get_or_create_default_minifi_container().files.append(File(os.path.join(path, str(uuid.uuid4())), ""))


@given('a host resource file "{filename}" is bound to the "{container_path}" path in the MiNiFi container "{container_name}"')
def step_impl(context: MinifiTestContext, filename: str, container_path: str, container_name: str):
    path = os.path.join(context.resource_dir, filename)
    context.get_or_create_minifi_container(container_name).add_host_file(path, container_path)


@given('a host resource file "{filename}" is bound to the "{container_path}" path in the MiNiFi container')
def step_impl(context: MinifiTestContext, filename: str, container_path: str):
    context.execute_steps(f"given a host resource file \"{filename}\" is bound to the \"{container_path}\" path in the MiNiFi container \"{DEFAULT_MINIFI_CONTAINER_NAME}\"")


@step("after {duration} have passed")
@step("after {duration} has passed")
def step_impl(context: MinifiTestContext, duration: str):
    time.sleep(humanfriendly.parse_timespan(duration))


@when("MiNiFi is stopped")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().stop()


@when("the \"{container_name}\" flow is stopped")
def step_impl(context: MinifiTestContext, container_name: str):
    context.get_or_create_minifi_container(container_name).stop()


@when("MiNiFi is restarted")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().restart()


@when("the \"{container_name}\" flow is restarted")
def step_impl(context: MinifiTestContext, container_name: str):
    context.get_or_create_minifi_container(container_name).restart()


@when("the \"{container_name}\" flow is started")
def step_impl(context: MinifiTestContext, container_name: str):
    context.get_or_create_minifi_container(container_name).start()


@when("the \"{container_name}\" flow is killed")
def step_impl(context: MinifiTestContext, container_name: str):
    context.get_or_create_minifi_container(container_name).kill()


@step("the http proxy server is set up")
def step_impl(context: MinifiTestContext):
    context.containers["http-proxy"] = HttpProxy(context)


@step("a NiFi container is set up")
def step_impl(context: MinifiTestContext):
    context.containers["nifi"] = NifiContainer(context)


@step("a NiFi container is set up with SSL enabled")
def step_impl(context: MinifiTestContext):
    context.containers["nifi"] = NifiContainer(context, use_ssl=True)


@when('NiFi is started')
def step_impl(context):
    assert context.containers["nifi"].deploy() or context.containers["nifi"].log_app_output()


@step("{duration} later")
def step_impl(context: MinifiTestContext, duration: str):
    time.sleep(humanfriendly.parse_timespan(duration))


@step("the MiNiFi deployment timeout is set to {timeout_seconds:d} seconds")
def step_impl(context: MinifiTestContext, timeout_seconds: int):
    context.get_or_create_default_minifi_container().set_deploy_timeout_seconds(timeout_seconds)
