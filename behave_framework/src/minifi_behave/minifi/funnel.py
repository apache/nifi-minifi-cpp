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


class Funnel:
    def __init__(self, funnel_name: str):
        self.name = funnel_name
        self.id: str = str(uuid.uuid4())

    def __repr__(self):
        return f"({self.name} {self.id})"

    def to_yaml_dict(self) -> dict:
        # Funnels have a simpler representation in the MiNiFi YAML
        return {'id': self.id, 'name': self.name, }
