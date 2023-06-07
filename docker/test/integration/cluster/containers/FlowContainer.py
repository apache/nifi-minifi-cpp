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


from .Container import Container


class FlowContainer(Container):
    def __init__(self, feature_context, config_dir, name, engine, vols, network, image_store, command):
        super().__init__(feature_context=feature_context,
                         name=name,
                         engine=engine,
                         vols=vols,
                         network=network,
                         image_store=image_store,
                         command=command)
        self.start_nodes = []
        self.config_dir = config_dir
        self.controllers = []

    def get_start_nodes(self):
        return self.start_nodes

    def add_start_node(self, node):
        self.start_nodes.append(node)

    def add_controller(self, controller):
        self.controllers.append(controller)
