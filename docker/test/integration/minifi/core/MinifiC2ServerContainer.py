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


class MinifiC2ServerContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None, ssl=False):
        engine = "minifi-c2-server-ssl" if ssl else "minifi-c2-server"
        super().__init__(name, engine, vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Server Started"

    def deploy(self):
        if not self.set_deployed():
            return

        self.docker_container = self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            network=self.network.name,
            entrypoint=self.command)
