#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

import uuid


class ControllerService:
    def __init__(self, class_name: str, service_name: str):
        self.class_name = class_name
        self.id: str = str(uuid.uuid4())
        self.name = service_name

        self.properties: dict[str, str] = {}

    def add_property(self, property_name: str, property_value: str):
        self.properties[property_name] = property_value

    def to_yaml_dict(self) -> dict:
        data = {
            "name": self.name,
            "id": self.id,
            "class": self.class_name,
            "Properties": self.properties,
        }

        return data
