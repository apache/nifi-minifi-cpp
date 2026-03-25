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
import os
from minifi_behave.containers.file import File
from minifi_behave.containers.host_file import HostFile
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.core.ssl_utils import make_cert_without_extended_usage, make_client_cert, make_server_cert, \
    dump_cert, dump_key
from minifi_behave.minifi.minifi_flow_definition import MinifiFlowDefinition
from pathlib import Path

from .container_linux import LinuxContainer
from .minifi_controller import MinifiController
from .minifi_protocol import MinifiProtocol


class NormalDeployment:
    def __init__(self):
        self.conf_path = "/opt/minifi/minifi-current/conf"
        self.bin_path = "/opt/minifi/minifi-current/bin"
        self.extension_pattern = "../extensions/*"
        self.minifi_python_path = "/opt/minifi/minifi-current/minifi-python"
        self.minifi_python_examples_path = "/opt/minifi/minifi-current/minifi-python-examples"


class FHSDeployment:
    def __init__(self):
        self.conf_path = "/etc/nifi-minifi-cpp/"
        self.bin_path = "/usr/bin"
        self.extension_pattern = "/usr/lib64/nifi-minifi-cpp/extensions/*"
        self.minifi_python_path = "/var/lib/nifi-minifi-cpp/minifi-python"
        self.minifi_python_examples_path = "/usr/share/doc/nifi-minifi-cpp/pythonprocessor-examples"


class MinifiLinuxContainer(LinuxContainer, MinifiProtocol):
    def __init__(self, container_name: str, test_context: MinifiTestContext,
                 deployment: NormalDeployment | FHSDeployment):
        super().__init__(test_context.minifi_container_image, f"{container_name}-{test_context.scenario_id}",
                         test_context.network)
        self.flow_definition = MinifiFlowDefinition()
        self.properties: dict[str, str] = {}
        self.log_properties: dict[str, str] = {}
        self.scenario_id = test_context.scenario_id
        self.deploy_timeout_seconds = 20
        self.deployment_type = deployment

        self.controller = MinifiController(self, f"{self.deployment_type.conf_path}/config.yml",
                                           self.deployment_type.bin_path)

        minifi_client_cert, minifi_client_key = make_cert_without_extended_usage(common_name=self.container_name,
                                                                                 ca_cert=test_context.root_ca_cert,
                                                                                 ca_key=test_context.root_ca_key)
        self.files.append(File("/usr/local/share/certs/ca-root-nss.crt", dump_cert(test_context.root_ca_cert)))
        self.files.append(File("/tmp/resources/root_ca.crt", dump_cert(test_context.root_ca_cert)))
        self.files.append(File("/tmp/resources/minifi_client.crt", dump_cert(minifi_client_cert)))
        self.files.append(File("/tmp/resources/minifi_client.key", dump_key(minifi_client_key)))
        self.files.append(
            File("/tmp/resources/minifi_merged_cert.crt", dump_cert(minifi_client_cert) + dump_key(minifi_client_key)))

        clientuser_cert, clientuser_key = make_client_cert("clientuser", ca_cert=test_context.root_ca_cert,
                                                           ca_key=test_context.root_ca_key)
        self.files.append(File("/tmp/resources/clientuser.crt", dump_cert(clientuser_cert)))
        self.files.append(File("/tmp/resources/clientuser.key", dump_key(clientuser_key)))

        minifi_server_cert, minifi_server_key = make_server_cert(common_name=f"server-{test_context.scenario_id}",
                                                                 ca_cert=test_context.root_ca_cert,
                                                                 ca_key=test_context.root_ca_key)
        self.files.append(
            File("/tmp/resources/minifi_server.crt", dump_cert(cert=minifi_server_cert) + dump_key(minifi_server_key)))

        self._fill_default_properties()
        self._fill_default_log_properties()

    def deploy(self, context: MinifiTestContext | None) -> bool:
        flow_config = self.flow_definition.to_yaml()
        logging.info(f"Deploying MiNiFi container '{self.container_name}' with flow configuration:\n{flow_config}")
        self.files.append(File(f"{self.deployment_type.conf_path}/config.yml", flow_config))
        self.files.append(
            File(f"{self.deployment_type.conf_path}/minifi.properties", self._get_properties_file_content()))
        self.files.append(
            File(f"{self.deployment_type.conf_path}/minifi-log.properties", self._get_log_properties_file_content()))
        resource_dir = Path(__file__).resolve().parent / "resources" / "minifi-controller"
        self.host_files.append(
            HostFile("/tmp/resources/minifi-controller/config.yml", os.path.join(resource_dir, "config.yml")))

        if not super().deploy(context):
            return False

        finished_str = "MiNiFi started"
        return wait_for_condition(condition=lambda: finished_str in self.get_logs(),
                                  timeout_seconds=self.deploy_timeout_seconds, bail_condition=lambda: self.exited,
                                  context=context)

    def set_deploy_timeout_seconds(self, timeout_seconds: int):
        self.deploy_timeout_seconds = timeout_seconds

    def set_property(self, key: str, value: str):
        self.properties[key] = value

    def set_log_property(self, key: str, value: str):
        self.log_properties[key] = value

    def _fill_default_properties(self):
        self.properties["nifi.flow.configuration.file"] = f"{self.deployment_type.conf_path}/config.yml"
        self.properties["nifi.extension.path"] = self.deployment_type.extension_pattern
        self.properties["nifi.administrative.yield.duration"] = "1 sec"
        self.properties["nifi.bored.yield.duration"] = "100 millis"
        self.properties["nifi.openssl.fips.support.enable"] = "false"
        self.properties["nifi.provenance.repository.class.name"] = "NoOpRepository"
        self.properties["nifi.python.processor.dir"] = self.deployment_type.minifi_python_path

    def _fill_default_log_properties(self):
        self.log_properties["spdlog.pattern"] = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"

        self.log_properties["appender.stderr"] = "stderr"
        self.log_properties["logger.root"] = "DEBUG, stderr"
        self.log_properties["logger.org::apache::nifi::minifi"] = "DEBUG, stderr"

    def _get_properties_file_content(self):
        lines = (f"{key}={value}" for key, value in self.properties.items())
        return "\n".join(lines)

    def _get_log_properties_file_content(self):
        lines = (f"{key}={value}" for key, value in self.log_properties.items())
        return "\n".join(lines)

    def add_example_python_processors(self):
        run_minifi_cmd = f'{self.deployment_type.bin_path}/minifi.sh run'
        self.command = f'sh -c "cp -r {self.deployment_type.minifi_python_examples_path} {self.deployment_type.minifi_python_path}/examples && {run_minifi_cmd}"'
