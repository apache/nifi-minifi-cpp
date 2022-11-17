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
from ..core.ControllerService import ControllerService


class GCPCredentialsControllerService(ControllerService):
    def __init__(self, name=None, credentials_location=None, json_path=None, raw_json=None):
        super(GCPCredentialsControllerService, self).__init__(name=name)

        self.service_class = 'GCPCredentialsControllerService'

        if credentials_location is not None:
            self.properties['Credentials Location'] = credentials_location

        if json_path is not None:
            self.properties['Service Account JSON File'] = json_path

        if raw_json is not None:
            self.properties['Service Account JSON'] = raw_json
