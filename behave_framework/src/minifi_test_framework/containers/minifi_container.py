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
from OpenSSL import crypto

from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.containers.file import File
from minifi_test_framework.minifi.flow_definition import FlowDefinition
from minifi_test_framework.core.ssl_utils import make_cert_without_extended_usage, make_client_cert
from .container import Container


class MinifiContainer(Container):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(test_context.minifi_container_image, f"{container_name}-{test_context.scenario_id}", test_context.network)
        self.flow_config_str: str = ""
        self.flow_definition = FlowDefinition()
        self.properties: dict[str, str] = {}
        self.log_properties: dict[str, str] = {}

        minifi_client_cert, minifi_client_key = make_cert_without_extended_usage(common_name=self.container_name, ca_cert=test_context.root_ca_cert, ca_key=test_context.root_ca_key)
        self.files.append(File("/tmp/resources/root_ca.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)))
        self.files.append(File("/tmp/resources/minifi_client.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=minifi_client_cert)))
        self.files.append(File("/tmp/resources/minifi_client.key", crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=minifi_client_key)))

        clientuser_cert, clientuser_key = make_client_cert("clientuser", ca_cert=test_context.root_ca_cert, ca_key=test_context.root_ca_key)
        self.files.append(File("/tmp/resources/clientuser.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=clientuser_cert)))
        self.files.append(File("/tmp/resources/clientuser.key", crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=clientuser_key)))

        self.is_fhs = 'MINIFI_INSTALLATION_TYPE=FHS' in str(self.client.images.get(test_context.minifi_container_image).history())

        self._fill_default_properties()
        self._fill_default_log_properties()

    def deploy(self) -> bool:
        flow_config = self.flow_definition.to_yaml()
        logging.info(f"Deploying MiNiFi container '{self.container_name}' with flow configuration:\n{flow_config}")
        if self.is_fhs:
            self.files.append(File("/etc/nifi-minifi-cpp/config.yml", flow_config))
            self.files.append(File("/etc/nifi-minifi-cpp/minifi.properties", self._get_properties_file_content()))
            self.files.append(
                File("/etc/nifi-minifi-cpp/minifi-log.properties", self._get_log_properties_file_content()))
        else:
            self.files.append(File("/opt/minifi/minifi-current/conf/config.yml", flow_config))
            self.files.append(
                File("/opt/minifi/minifi-current/conf/minifi.properties", self._get_properties_file_content()))
            self.files.append(File("/opt/minifi/minifi-current/conf/minifi-log.properties",
                                   self._get_log_properties_file_content()))

        return super().deploy()

    def set_property(self, key: str, value: str):
        self.properties[key] = value

    def set_log_property(self, key: str, value: str):
        self.log_properties[key] = value

    def enable_openssl_fips_mode(self):
        self.properties["nifi.openssl.fips.support.enable"] = "true"

    def _fill_default_properties(self):
        if self.is_fhs:
            self.properties["nifi.flow.configuration.file"] = "/etc/nifi-minifi-cpp/config.yml"
            self.properties["nifi.extension.path"] = "/usr/lib64/nifi-minifi-cpp/extensions/*"
        else:
            self.properties["nifi.flow.configuration.file"] = "./conf/config.yml"
            self.properties["nifi.extension.path"] = "../extensions/*"
        self.properties["nifi.administrative.yield.duration"] = "1 sec"
        self.properties["nifi.bored.yield.duration"] = "100 millis"
        self.properties["nifi.openssl.fips.support.enable"] = "false"
        self.properties["nifi.provenance.repository.class.name"] = "NoOpRepository"

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

    def get_memory_usage(self) -> int | None:
        exit_code, output = self.exec_run(["awk", "/VmRSS/ { printf \"%d\\n\", $2 }", "/proc/1/status"])
        if exit_code != 0:
            return None
        memory_usage_in_bytes = int(output.strip()) * 1024
        logging.info(f"MiNiFi memory usage: {memory_usage_in_bytes} bytes")
        return memory_usage_in_bytes
