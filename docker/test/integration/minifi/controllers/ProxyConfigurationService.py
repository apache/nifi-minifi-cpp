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


class ProxyConfigurationService(ControllerService):
    def __init__(self, name, host, port=None, username=None, password=None):
        super(ProxyConfigurationService, self).__init__(name=name)

        self.service_class = 'ProxyConfigurationService'

        self.properties['Proxy Server Host'] = host

        if port is not None:
            self.properties['Proxy Server Port'] = port

        if username is not None:
            self.properties['Proxy User Name'] = username

        if password is not None:
            self.properties['Proxy User Password'] = password
