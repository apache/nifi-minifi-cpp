from typing import Dict

from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.minifi.minifi_flow_definition import MinifiFlowDefinition
from minifi_test_framework.containers.directory import Directory
from .container_windows import WindowsContainer
from .minifi_protocol import MinifiProtocol
import logging


class MinifiWindowsContainer(WindowsContainer, MinifiProtocol):
    def __init__(self, container_name: str, test_context: MinifiTestContext):
        super().__init__(test_context.minifi_container_image, f"{container_name}-{test_context.scenario_id}", test_context.network)
        self.flow_config_str: str = ""
        self.flow_definition = MinifiFlowDefinition()
        self.properties: Dict[str, str] = {}
        self.log_properties: Dict[str, str] = {}

        self._fill_default_properties()
        self._fill_default_log_properties()

    def deploy(self, context: MinifiTestContext | None) -> bool:
        logging.info(self.flow_definition.to_yaml())
        conf_dir = Directory("\\Program Files\\ApacheNiFiMiNiFi\\nifi-minifi-cpp\\conf")
        conf_dir.add_file("config.yml", self.flow_definition.to_yaml())
        conf_dir.add_file("minifi.properties", self._get_properties_file_content())
        conf_dir.add_file("minifi-log.properties", self._get_log_properties_file_content())

        self.dirs.append(conf_dir)
        return super().deploy(context)

    def set_property(self, key: str, value: str):
        self.properties[key] = value

    def set_log_property(self, key: str, value: str):
        self.log_properties[key] = value

    def _fill_default_properties(self):
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
