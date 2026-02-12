import logging
from minifi_test_framework.core.helpers import retry_check


class MinifiController:
    def __init__(self, minifi_container, config_path):
        self.minifi_container = minifi_container
        self.config_path = config_path

    def set_controller_socket_properties(self):
        self.minifi_container.properties["controller.socket.enable"] = "true"
        self.minifi_container.properties["controller.socket.host"] = "localhost"
        self.minifi_container.properties["controller.socket.port"] = "9998"
        self.minifi_container.properties["controller.socket.local.any.interface"] = "false"

    def update_flow_config_through_controller(self):
        self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--updateflow", "/tmp/resources/minifi-controller/config.yml"])

    def updated_config_is_persisted(self) -> bool:
        exit_code, output = self.minifi_container.exec_run(["cat", self.config_path])
        if exit_code != 0:
            logging.error("Failed to read MiNiFi config file to check if updated config is persisted")
            return False
        return "2f2a3b47-f5ba-49f6-82b5-bc1c86b96f38" in output

    def stop_component_through_controller(self, component: str):
        self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--stop", component])

    def start_component_through_controller(self, component: str):
        self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--start", component])

    @retry_check(10, 1)
    def is_component_running(self, component: str) -> bool:
        (code, output) = self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--list", "components"])
        return code == 0 and component + ", running: true" in output

    def get_connections(self):
        (_, output) = self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--list", "connections"])
        connections = []
        for line in output.split('\n'):
            if not line.startswith('[') and not line.startswith('Connection Names'):
                connections.append(line)
        return connections

    @retry_check(10, 1)
    def connection_found_through_controller(self, connection: str) -> bool:
        return connection in self.get_connections()

    def get_full_connection_count(self) -> int:
        (_, output) = self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--getfull"])
        for line in output.split('\n'):
            if "are full" in line:
                return int(line.split(' ')[0])
        return -1

    def get_connection_size(self, connection: str):
        (_, output) = self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--getsize", connection])
        for line in output.split('\n'):
            if "Size/Max of " + connection in line:
                size_and_max = line.split(connection)[1].split('/')
                return (int(size_and_max[0].strip()), int(size_and_max[1].strip()))
        return (-1, -1)

    def get_manifest(self) -> str:
        (_, output) = self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--manifest"])
        manifest = ""
        for line in output.split('\n'):
            if not line.startswith('['):
                manifest += line
        return manifest

    def create_debug_bundle(self) -> bool:
        (code, _) = self.minifi_container.exec_run([self.minifi_container.minifi_controller_path, "--debug", "/tmp"])
        if code != 0:
            logging.error("Minifi controller debug command failed with code: %d", code)
            return False

        (code, _) = self.minifi_container.exec_run(["test", "-f", "/tmp/debug.tar.gz"])
        return code == 0
