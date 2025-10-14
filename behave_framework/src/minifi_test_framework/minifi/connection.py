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


class Connection:
    def __init__(self, source_name: str, source_relationship: str, target_name: str):
        self.id: str = str(uuid.uuid4())
        self.source_name: str = source_name
        self.source_relationship: str = source_relationship
        self.target_name: str = target_name

    def __repr__(self):
        return f"({self.source_name}:{self.source_relationship} --> {self.target_name})"
