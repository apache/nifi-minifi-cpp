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
import uuid
from .remote_port import RemotePort


class RemoteProcessGroup:
    def __init__(self, name: str, address: str, protocol: str):
        self.id: str = str(uuid.uuid4())
        self.name: str = name
        self.address: str = address
        self.protocol: str = protocol
        self.input_ports: list[RemotePort] = []
        self.output_ports: list[RemotePort] = []
        self.properties: dict[str, str] = {}

    def add_input_port(self, port_name: str, use_compression: bool = False):
        self.input_ports.append(RemotePort(port_name, use_compression))

    def add_output_port(self, port_name: str, use_compression: bool = False):
        self.output_ports.append(RemotePort(port_name, use_compression))

    def get_input_port(self, port_name: str) -> RemotePort | None:
        return next((port for port in self.input_ports if port.name == port_name), None)

    def to_yaml_dict(self):
        data = {'id': self.id, 'name': self.name, 'timeout': '30 sec', 'transport protocol': self.protocol,
                'url': self.address, 'yield period': '3 sec', 'Input Ports': [port.to_yaml_dict() for port in self.input_ports],
                'Output Ports': [port.to_yaml_dict() for port in self.output_ports]}
        return data
