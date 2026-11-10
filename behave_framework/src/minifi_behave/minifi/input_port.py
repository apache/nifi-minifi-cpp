#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#


class InputPort:
    def __init__(self, name: str, id: str):
        self.id: str = id
        self.name: str = name

    def to_json_dict(self):
        data = {
            "identifier": self.id,
            "instanceIdentifier": self.id,
            "name": self.name,
            "comments": "",
            "position": {"x": 0, "y": 0},
            "type": "INPUT_PORT",
            "concurrentlySchedulableTaskCount": 1,
            "scheduledState": "RUNNING",
            "allowRemoteAccess": True,
            "portFunction": "STANDARD",
            "componentType": "INPUT_PORT",
            "groupIdentifier": "9802c873-3322-3b60-a71d-732d02bd60f8",
        }
        return data
