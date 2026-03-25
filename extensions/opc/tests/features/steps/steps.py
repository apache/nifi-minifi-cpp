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

import logging
import os
import tempfile
import docker
import io
import tarfile
import humanfriendly

from behave import given, step, then
from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.helpers import wait_for_condition
from containers.opc_ua_server_container import OPCUAServerContainer


@step("an OPC UA server is set up")
def setup_opcua_server(context: MinifiTestContext):
    context.containers["opcua-server"] = OPCUAServerContainer(context)


@step("an OPC UA server is set up with access control")
def setup_opcua_server_with_access_control(context: MinifiTestContext):
    context.containers["opcua-server-access"] = OPCUAServerContainer(context, command=["/opt/open62541/examples/access_control_server"])


@then("the OPC UA server logs contain the following message: \"{log_message}\" in less than {duration}")
def verify_opcua_server_logs_contain_message(context, log_message, duration):
    timeout_seconds = humanfriendly.parse_timespan(duration)
    opcua_container = context.containers["opcua-server"]
    assert isinstance(opcua_container, OPCUAServerContainer)
    assert wait_for_condition(
        condition=lambda: log_message in opcua_container.get_logs(),
        timeout_seconds=timeout_seconds,
        bail_condition=lambda: opcua_container.exited,
        context=context)


def _copy_file_from_docker_image(image_name: str, file_path: str, output_path: str):
    docker_client: docker.DockerClient = docker.from_env()
    container = docker_client.containers.create(image_name)

    try:
        bits, _ = container.get_archive(file_path)
        tar_stream = io.BytesIO(b"".join(bits))
        with tarfile.open(fileobj=tar_stream) as tar:
            member = tar.getmembers()[0]
            file_content = tar.extractfile(member).read()

        with open(output_path, "wb") as f:
            f.write(file_content)

        return True
    except Exception as e:
        logging.error(f"Error copying file {file_path} from Docker image {image_name}: {e}")
        return False
    finally:
        container.remove(force=True)


@given('the OPC UA server certificate files are placed in the "{directory}" directory in the MiNiFi container "{container_name}"')
def place_opcua_certificate_files_in_minifi_container(context: MinifiTestContext, directory: str, container_name: str):
    if not hasattr(context, "opcua_cert_temp_dir"):
        context.opcua_cert_temp_dir = tempfile.TemporaryDirectory()
        _copy_file_from_docker_image(OPCUAServerContainer.OPC_SERVER_IMAGE, "/opt/open62541/pki/created/server_cert.der", os.path.join(context.opcua_cert_temp_dir.name, "server_cert.der"))
        _copy_file_from_docker_image(OPCUAServerContainer.OPC_SERVER_IMAGE, "/opt/open62541/pki/created/server_key.der", os.path.join(context.opcua_cert_temp_dir.name, "server_key.der"))

    context.get_or_create_minifi_container(container_name).add_host_file(os.path.join(context.opcua_cert_temp_dir.name, "server_cert.der"), os.path.join(directory, "opcua_client_cert.der"))
    context.get_or_create_minifi_container(container_name).add_host_file(os.path.join(context.opcua_cert_temp_dir.name, "server_key.der"), os.path.join(directory, "opcua_client_key.der"))
