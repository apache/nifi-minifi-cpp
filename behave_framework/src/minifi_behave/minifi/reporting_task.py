#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

import uuid


class ReportingTask:
    def __init__(
        self, class_name: str, name: str, scheduling_strategy: str = "TIMER_DRIVEN", scheduling_period: str = "1 sec"
    ):
        self.class_name = class_name
        self.id: str = str(uuid.uuid4())
        self.name = name
        self.scheduling_strategy: str = scheduling_strategy
        self.scheduling_period: str = scheduling_period
        self.properties: dict[str, str] = {}

    def add_property(self, property_name: str, property_value: str):
        self.properties[property_name] = property_value

    def remove_property(self, property_name: str):
        if property_name in self.properties:
            del self.properties[property_name]

    def __repr__(self):
        return f"ReportingTask({self.class_name}, {self.name}, {self.properties})"

    def to_yaml_dict(self) -> dict:
        return {
            "name": self.name,
            "id": self.id,
            "class": self.class_name,
            "scheduling strategy": self.scheduling_strategy,
            "scheduling period": self.scheduling_period,
            "Properties": self.properties,
        }
