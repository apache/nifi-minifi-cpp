# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import uuid


class Connectable(object):
    def __init__(self,
                 name=None,
                 auto_terminate=None):

        self.uuid = uuid.uuid4()

        if name is None:
            self.name = "node_of_" + str(self.uuid)
        else:
            self.name = name

        if auto_terminate is None:
            self.auto_terminate = []
        else:
            self.auto_terminate = auto_terminate

        self.connections = {}
        self.out_proc = self

        self.drop_empty_flowfiles = False

    def connect(self, connections):
        for rel in connections:

            # Ensure that rel is not auto-terminated
            if rel in self.auto_terminate:
                del self.auto_terminate[self.auto_terminate.index(rel)]

            # Add to set of output connections for this rel
            if rel not in self.connections:
                self.connections[rel] = []
            self.connections[rel].append(connections[rel])

        return self

    def get_name(self):
        return self.name

    def set_name(self, name):
        self.name = name

    def get_uuid(self):
        return self.uuid

    def set_uuid(self, uuid):
        self.uuid = uuid
