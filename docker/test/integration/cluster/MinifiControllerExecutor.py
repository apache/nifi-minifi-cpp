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
from .DockerCommunicator import DockerCommunicator
from .containers.MinifiContainer import MinifiContainer


class MinifiControllerExecutor:
    def __init__(self, container_communicator: DockerCommunicator):
        self.container_communicator = container_communicator

    def update_flow(self, container_name: str):
        self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--updateflow", "/tmp/resources/minifi-controller/config.yml"])

    def updated_config_is_persisted(self, container_name: str) -> bool:
        (code, output) = self.container_communicator.execute_command(container_name, ["cat", MinifiContainer.MINIFI_LOCATIONS.config_path])
        return code == 0 and "2f2a3b47-f5ba-49f6-82b5-bc1c86b96f38" in output

    def stop_component(self, component: str, container_name: str):
        self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--stop", component])

    def start_component(self, component: str, container_name: str):
        self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--start", component])

    def is_component_running(self, component: str, container_name: str) -> bool:
        (code, output) = self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--list", "components"])
        return code == 0 and component + ", running: true" in output

    def get_connections(self, container_name: str):
        (_, output) = self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--list", "connections"])
        connections = []
        for line in output.split('\n'):
            if not line.startswith('[') and not line.startswith('Connection Names'):
                connections.append(line)
        return connections

    def get_full_connection_count(self, container_name: str) -> int:
        (_, output) = self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--getfull"])
        for line in output.split('\n'):
            if "are full" in line:
                return int(line.split(' ')[0])
        return -1

    def get_connection_size(self, connection: str, container_name: str):
        (_, output) = self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--getsize", connection])
        for line in output.split('\n'):
            if "Size/Max of " + connection in line:
                size_and_max = line.split(connection)[1].split('/')
                return (int(size_and_max[0].strip()), int(size_and_max[1].strip()))
        return (-1, -1)

    def get_manifest(self, container_name: str) -> str:
        (_, output) = self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--manifest"])
        manifest = ""
        for line in output.split('\n'):
            if not line.startswith('['):
                manifest += line
        return manifest

    def get_debug_bundle(self, container_name: str, dest: str) -> bool:
        (code, _) = self.container_communicator.execute_command(container_name, [MinifiContainer.MINIFI_LOCATIONS.minifi_controller_path, "--debug", "/tmp"])
        if code != 0:
            logging.error("Minifi controller debug command failed with code: %d", code)
            return False

        return self.container_communicator.copy_file_from_container(container_name, "/tmp/debug.tar.gz", dest)
