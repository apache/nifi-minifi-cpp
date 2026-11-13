#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#
import uuid


class RemotePort:
    def __init__(self, name: str, use_compression: bool):
        self.id: str = str(uuid.uuid4())
        self.name: str = name
        self.use_compression: bool = use_compression
        self.properties: dict[str, str] = {}

    def add_property(self, property_name: str, property_value: str):
        self.properties[property_name] = property_value

    def to_yaml_dict(self):
        data = {
            "id": self.id,
            "name": self.name,
            "use compression": self.use_compression,
            "max concurrent tasks": 1,
            "Properties": self.properties,
        }
        return data
