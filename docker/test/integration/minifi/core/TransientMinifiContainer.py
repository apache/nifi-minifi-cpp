# @file TransientMinifiContainer.py
#
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

from .MinifiContainer import MinifiContainer


class TransientMinifiContainer(MinifiContainer):
    def __init__(self, config_dir, name, vols, network, image_store, command=None):
        if not command:
            command = ["/bin/sh", "-c",
                       "cp /tmp/minifi_config/config.yml ./conf/ && ./bin/minifi.sh start && sleep 10 && ./bin/minifi.sh stop && sleep 100"]
        super().__init__(config_dir, name, vols, network, image_store, command)
