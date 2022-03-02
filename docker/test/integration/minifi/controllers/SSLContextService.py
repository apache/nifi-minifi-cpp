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


class SSLContextService(ControllerService):
    def __init__(self, name=None, cert=None, key=None, ca_cert=None, passphrase=None):
        super(SSLContextService, self).__init__(name=name)

        self.service_class = 'SSLContextService'

        if cert is not None:
            self.properties['Client Certificate'] = cert

        if key is not None:
            self.properties['Private Key'] = key

        if ca_cert is not None:
            self.properties['CA Certificate'] = ca_cert

        if passphrase is not None:
            self.properties['Passphrase'] = passphrase
