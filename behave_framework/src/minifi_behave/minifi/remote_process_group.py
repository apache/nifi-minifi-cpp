#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
        data = {
            "id": self.id,
            "name": self.name,
            "timeout": "30 sec",
            "transport protocol": self.protocol,
            "url": self.address,
            "yield period": "3 sec",
            "Input Ports": [port.to_yaml_dict() for port in self.input_ports],
            "Output Ports": [port.to_yaml_dict() for port in self.output_ports],
        }
        return data
