#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

import uuid

from minifi_behave.minifi.parameter import Parameter


class ParameterContext:
    def __init__(self, name: str):
        self.name = name
        self.id: str = str(uuid.uuid4())
        self.parameters: list[Parameter] = []

    def to_yaml_dict(self) -> dict:
        return {
            "id": self.id,
            "name": self.name,
            "Parameters": [p.to_yaml_dict() for p in self.parameters],
        }
