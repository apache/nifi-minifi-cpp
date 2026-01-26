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


class RemotePort:
    def __init__(self, name: str, use_compression: bool):
        self.id: str = str(uuid.uuid4())
        self.name: str = name
        self.use_compression: bool = use_compression
        self.properties: dict[str, str] = {}

    def add_property(self, property_name: str, property_value: str):
        self.properties[property_name] = property_value

    def to_yaml_dict(self):
        data = {'id': self.id, 'name': self.name, 'use compression': self.use_compression, 'max concurrent tasks': 1,
                'Properties': self.properties}
        return data
