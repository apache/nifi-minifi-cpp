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


from ..core.Processor import Processor


class InvokeHTTP(Processor):
    def __init__(
            self,
            ssl_context_service=None,
            schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        properties = {
            "Proxy Host": "",
            "Proxy Port": "",
            "invokehttp-proxy-username": "",
            "invokehttp-proxy-password": ""
        }

        controller_services = []

        if ssl_context_service is not None:
            properties['SSL Context Service'] = ssl_context_service.name
            controller_services.append(ssl_context_service)

        super(InvokeHTTP, self).__init__(
            'InvokeHTTP',
            properties=properties,
            controller_services=controller_services,
            auto_terminate=['success', 'response', 'retry', 'failure', 'no retry'],
            schedule=schedule)
        self.out_proc.connect({"failure": self})
