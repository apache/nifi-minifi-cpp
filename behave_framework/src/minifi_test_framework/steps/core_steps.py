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
from minifi_test_framework.containers.minifi_c2_server_container import MinifiC2Server


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
    new_dir = Directory(directory)
    new_dir.files["input.txt"] = content
    context.get_or_create_default_minifi_container().dirs.append(new_dir)


@step('a file with filename "{file_name}" and content "{content}" is present in "{path}"')
def step_impl(context: MinifiTestContext, file_name: str, content: str, path: str):
    new_content = content.replace("\\n", "\n")
    context.get_or_create_default_minifi_container().files.append(File(os.path.join(path, file_name), new_content))


@step('a file with the content "{content}" is present in "{path}"')
def step_impl(context: MinifiTestContext, content: str, path: str):
    new_content = content.replace("\\n", "\n")
    context.get_or_create_default_minifi_container().files.append(File(os.path.join(path, str(uuid.uuid4())), new_content))


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


@when("MiNiFi is restarted")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().restart()


@given("OpenSSL FIPS mode is enabled in MiNiFi")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().enable_openssl_fips_mode()


@step("the http proxy server is set up")
def step_impl(context: MinifiTestContext):
    context.containers["http-proxy"] = HttpProxy(context)


@step("a NiFi container is set up")
def step_impl(context: MinifiTestContext):
    context.containers["nifi"] = NifiContainer(context)


@given("C2 is enabled in MiNiFi")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().enable_c2()


@given("flow configuration path is set up in flow url property")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().fetch_flow_config_from_flow_url()


@given("ssl properties are set up for MiNiFi C2 server")
def step_impl(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().enable_c2_with_ssl()
    context.get_or_create_default_minifi_container().set_up_ssl_proprties()


@given("a MiNiFi C2 server is set up")
def step_impl(context: MinifiTestContext):
    context.containers["minifi-c2-server"] = MinifiC2Server(context)


@given("a MiNiFi C2 server is set up with SSL")
def step_impl(context: MinifiTestContext):
    context.containers["minifi-c2-server"] = MinifiC2Server(context, ssl=True)


@given("a MiNiFi C2 server is started")
def step_impl(context: MinifiTestContext):
    context.containers["minifi-c2-server"] = MinifiC2Server(context)
    assert context.containers["minifi-c2-server"].deploy()
