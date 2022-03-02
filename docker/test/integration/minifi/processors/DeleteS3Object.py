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


class DeleteS3Object(Processor):
    def __init__(
            self,
            proxy_host='',
            proxy_port='',
            proxy_username='',
            proxy_password=''):
        super(DeleteS3Object, self).__init__(
            'DeleteS3Object',
            properties={
                'Object Key': 'test_object_key',
                'Bucket': 'test_bucket',
                'Access Key': 'test_access_key',
                'Secret Key': 'test_secret',
                'Endpoint Override URL': "http://s3-server:9090",
                'Proxy Host': proxy_host,
                'Proxy Port': proxy_port,
                'Proxy Username': proxy_username,
                'Proxy Password': proxy_password,
            },
            auto_terminate=['success'])
