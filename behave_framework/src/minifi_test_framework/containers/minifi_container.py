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
from pathlib import Path
from OpenSSL import crypto

from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.host_file import HostFile
from minifi_test_framework.minifi.minifi_flow_definition import MinifiFlowDefinition
from minifi_test_framework.core.ssl_utils import make_cert_without_extended_usage, make_client_cert, make_server_cert
from minifi_test_framework.core.helpers import wait_for_condition, retry_check
from .container import Container


class MinifiContainer(Container):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(test_context.minifi_container_image, f"{container_name}-{test_context.scenario_id}", test_context.network)
        self.flow_definition = MinifiFlowDefinition()
        self.properties: dict[str, str] = {}
        self.log_properties: dict[str, str] = {}
        self.scenario_id = test_context.scenario_id

        minifi_client_cert, minifi_client_key = make_cert_without_extended_usage(common_name=self.container_name, ca_cert=test_context.root_ca_cert, ca_key=test_context.root_ca_key)
        self.files.append(File("/usr/local/share/certs/ca-root-nss.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)))
        self.files.append(File("/tmp/resources/root_ca.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)))
        self.files.append(File("/tmp/resources/minifi_client.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=minifi_client_cert)))
        self.files.append(File("/tmp/resources/minifi_client.key", crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=minifi_client_key)))
        self.files.append(File("/tmp/resources/minifi_merged_cert.crt",
                               crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=minifi_client_cert) + crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=minifi_client_key)))

        clientuser_cert, clientuser_key = make_client_cert("clientuser", ca_cert=test_context.root_ca_cert, ca_key=test_context.root_ca_key)
        self.files.append(File("/tmp/resources/clientuser.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=clientuser_cert)))
        self.files.append(File("/tmp/resources/clientuser.key", crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=clientuser_key)))

        minifi_server_cert, minifi_server_key = make_server_cert(common_name=f"server-{test_context.scenario_id}", ca_cert=test_context.root_ca_cert, ca_key=test_context.root_ca_key)
        self.files.append(File("/tmp/resources/minifi_server.crt",
                               crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=minifi_server_cert) + crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=minifi_server_key)))

        self.is_fhs = 'MINIFI_INSTALLATION_TYPE=FHS' in str(self.client.images.get(test_context.minifi_container_image).history())
        if self.is_fhs:
            self.minifi_controller_path = '/usr/bin/minifi-controller'
        else:
            self.minifi_controller_path = '/opt/minifi/minifi-current/bin/minifi-controller'

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

        resource_dir = Path(__file__).resolve().parent / "resources" / "minifi-controller"
        self.host_files.append(HostFile("/tmp/resources/minifi-controller/config.yml", os.path.join(resource_dir, "config.yml")))

        if not super().deploy():
            return False

        finished_str = "MiNiFi started"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=15,
            bail_condition=lambda: self.exited,
            context=None)

    def set_property(self, key: str, value: str):
        self.properties[key] = value

    def set_log_property(self, key: str, value: str):
        self.log_properties[key] = value

    def enable_openssl_fips_mode(self):
        self.properties["nifi.openssl.fips.support.enable"] = "true"

    def enable_log_metrics_publisher(self):
        self.properties["nifi.metrics.publisher.LogMetricsPublisher.metrics"] = "RepositoryMetrics"
        self.properties["nifi.metrics.publisher.LogMetricsPublisher.logging.interval"] = "1s"
        self.properties["nifi.metrics.publisher.class"] = "LogMetricsPublisher"

    def enable_prometheus(self):
        self.properties["nifi.metrics.publisher.agent.identifier"] = "Agent1"
        self.properties["nifi.metrics.publisher.PrometheusMetricsPublisher.port"] = "9936"
        self.properties["nifi.metrics.publisher.PrometheusMetricsPublisher.metrics"] = "RepositoryMetrics,QueueMetrics,PutFileMetrics,processorMetrics/Get.*,FlowInformation,DeviceInfoNode,AgentStatus"
        self.properties["nifi.metrics.publisher.class"] = "PrometheusMetricsPublisher"

    def enable_prometheus_with_ssl(self):
        self.enable_prometheus()
        self.properties["nifi.metrics.publisher.PrometheusMetricsPublisher.certificate"] = "/tmp/resources/minifi_merged_cert.crt"
        self.properties["nifi.metrics.publisher.PrometheusMetricsPublisher.ca.certificate"] = "/tmp/resources/root_ca.crt"

    def fetch_flow_config_from_flow_url(self):
        self.properties["nifi.c2.flow.url"] = f"http://minifi-c2-server-{self.scenario_id}:10090/c2/config?class=minifi-test-class"

    def set_up_ssl_properties(self):
        self.properties["nifi.remote.input.secure"] = "true"
        self.properties["nifi.security.client.certificate"] = "/tmp/resources/minifi_client.crt"
        self.properties["nifi.security.client.private.key"] = "/tmp/resources/minifi_client.key"
        self.properties["nifi.security.client.ca.certificate"] = "/tmp/resources/root_ca.crt"

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

    def set_controller_socket_properties(self):
        self.properties["controller.socket.enable"] = "true"
        self.properties["controller.socket.host"] = "localhost"
        self.properties["controller.socket.port"] = "9998"
        self.properties["controller.socket.local.any.interface"] = "false"

    def update_flow_config_through_controller(self):
        self.exec_run([self.minifi_controller_path, "--updateflow", "/tmp/resources/minifi-controller/config.yml"])

    def updated_config_is_persisted(self) -> bool:
        exit_code, output = self.exec_run(["cat", "/opt/minifi/minifi-current/conf/config.yml" if not self.is_fhs else "/etc/nifi-minifi-cpp/config.yml"])
        if exit_code != 0:
            logging.error("Failed to read MiNiFi config file to check if updated config is persisted")
            return False
        return "2f2a3b47-f5ba-49f6-82b5-bc1c86b96f38" in output

    def stop_component_through_controller(self, component: str):
        self.exec_run([self.minifi_controller_path, "--stop", component])

    def start_component_through_controller(self, component: str):
        self.exec_run([self.minifi_controller_path, "--start", component])

    @retry_check(10, 1)
    def is_component_running(self, component: str) -> bool:
        (code, output) = self.exec_run([self.minifi_controller_path, "--list", "components"])
        return code == 0 and component + ", running: true" in output

    def get_connections(self):
        (_, output) = self.exec_run([self.minifi_controller_path, "--list", "connections"])
        connections = []
        for line in output.split('\n'):
            if not line.startswith('[') and not line.startswith('Connection Names'):
                connections.append(line)
        return connections

    @retry_check(10, 1)
    def connection_found_through_controller(self, connection: str) -> bool:
        return connection in self.get_connections()

    def get_full_connection_count(self) -> int:
        (_, output) = self.exec_run([self.minifi_controller_path, "--getfull"])
        for line in output.split('\n'):
            if "are full" in line:
                return int(line.split(' ')[0])
        return -1

    def get_connection_size(self, connection: str):
        (_, output) = self.exec_run([self.minifi_controller_path, "--getsize", connection])
        for line in output.split('\n'):
            if "Size/Max of " + connection in line:
                size_and_max = line.split(connection)[1].split('/')
                return (int(size_and_max[0].strip()), int(size_and_max[1].strip()))
        return (-1, -1)

    def get_manifest(self) -> str:
        (_, output) = self.exec_run([self.minifi_controller_path, "--manifest"])
        manifest = ""
        for line in output.split('\n'):
            if not line.startswith('['):
                manifest += line
        return manifest

    def create_debug_bundle(self) -> bool:
        (code, _) = self.exec_run([self.minifi_controller_path, "--debug", "/tmp"])
        if code != 0:
            logging.error("Minifi controller debug command failed with code: %d", code)
            return False

        (code, _) = self.exec_run(["test", "-f", "/tmp/debug.tar.gz"])
        return code == 0
