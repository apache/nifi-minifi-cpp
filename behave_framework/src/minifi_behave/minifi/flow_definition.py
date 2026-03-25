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

from abc import ABC

from .connection import Connection
from .controller_service import ControllerService
from .funnel import Funnel
from .parameter_context import ParameterContext
from .processor import Processor
from .remote_process_group import RemoteProcessGroup
from .input_port import InputPort
from .output_port import OutputPort


class FlowDefinition(ABC):
    def __init__(self, flow_name: str = "MiNiFi Flow"):
        self.flow_name = flow_name
        self.processors: list[Processor] = []
        self.controller_services: list[ControllerService] = []
        self.funnels: list[Funnel] = []
        self.connections: list[Connection] = []
        self.parameter_contexts: list[ParameterContext] = []
        self.remote_process_groups: list[RemoteProcessGroup] = []
        self.input_ports: list[InputPort] = []
        self.output_ports: list[OutputPort] = []

    def add_processor(self, processor: Processor):
        self.processors.append(processor)

    def add_remote_process_group(self, address: str, name: str, protocol: str = "RAW"):
        rpg = RemoteProcessGroup(name, address, protocol)
        self.remote_process_groups.append(rpg)

    def add_input_port_to_rpg(self, rpg_name: str, port_name: str, use_compression: bool = False):
        rpg = next((rpg for rpg in self.remote_process_groups if rpg.name == rpg_name), None)
        if rpg:
            rpg.add_input_port(port_name, use_compression)
        else:
            raise ValueError(f"RemoteProcessGroup with name '{rpg_name}' not found.")

    def add_output_port_to_rpg(self, rpg_name: str, port_name: str, use_compression: bool = False):
        rpg = next((rpg for rpg in self.remote_process_groups if rpg.name == rpg_name), None)
        if rpg:
            rpg.add_output_port(port_name, use_compression)
        else:
            raise ValueError(f"RemoteProcessGroup with name '{rpg_name}' not found.")

    def get_input_port_id_of_rpg(self, rpg_name: str, rpg_port_name: str) -> str:
        rpg = next((rpg for rpg in self.remote_process_groups if rpg.name == rpg_name), None)
        if rpg:
            port = next((port for port in rpg.input_ports if port.name == rpg_port_name), None)
            if port:
                return port.id
            else:
                raise ValueError(f"InputPort with name '{rpg_port_name}' not found in RPG '{rpg_name}'.")
        else:
            raise ValueError(f"RemoteProcessGroup with name '{rpg_name}' not found.")

    def get_output_port_id_of_rpg(self, rpg_name: str, rpg_port_name: str) -> str:
        rpg = next((rpg for rpg in self.remote_process_groups if rpg.name == rpg_name), None)
        if rpg:
            port = next((port for port in rpg.output_ports if port.name == rpg_port_name), None)
            if port:
                return port.id
            else:
                raise ValueError(f"OutputPort with name '{rpg_port_name}' not found in RPG '{rpg_name}'.")
        else:
            raise ValueError(f"RemoteProcessGroup with name '{rpg_name}' not found.")

    def add_input_port(self, input_port_id: str, input_port_name: str):
        self.input_ports.append(InputPort(input_port_name, input_port_id))

    def add_output_port(self, output_port_id: str, output_port_name: str):
        self.output_ports.append(OutputPort(output_port_name, output_port_id))

    def get_processor(self, processor_name: str) -> Processor | None:
        return next((proc for proc in self.processors if proc.name == processor_name), None)

    def get_controller_service(self, controller_service_name: str) -> ControllerService | None:
        return next((controller for controller in self.controller_services if controller.name == controller_service_name), None)

    def get_parameter_context(self, parameter_context_name: str) -> ParameterContext | None:
        return next((parameter_context for parameter_context in self.parameter_contexts if
                     parameter_context.name == parameter_context_name), None)

    def get_remote_process_group(self, rpg_name: str) -> RemoteProcessGroup | None:
        return next((rpg for rpg in self.remote_process_groups if rpg.name == rpg_name), None)

    def add_funnel(self, funnel: Funnel):
        self.funnels.append(funnel)

    def add_connection(self, connection: Connection):
        self.connections.append(connection)

    def set_drop_empty_for_destination(self, destination_name: str):
        for connection in self.connections:
            if connection.target_name == destination_name:
                connection.drop_empty_flowfiles = True

    def to_yaml(self) -> str:
        raise NotImplementedError("to_yaml method must be implemented in subclasses")

    def to_json(self) -> str:
        raise NotImplementedError("to_json method must be implemented in subclasses")

    def __repr__(self):
        return f"FlowDefinition(Processors: {self.processors}, Controller Services: {self.controller_services})"
