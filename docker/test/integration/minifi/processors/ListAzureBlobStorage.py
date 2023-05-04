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


class ListAzureBlobStorage(Processor):
    def __init__(self, context):
        connection_string = 'DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint=http://azure-storage-server-{feature_id}:10000/devstoreaccount1;QueueEndpoint=http://azure-storage-server-{feature_id}:10001/devstoreaccount1;'.format(feature_id=context.feature_id)
        super(ListAzureBlobStorage, self).__init__(
            context=context,
            clazz='ListAzureBlobStorage',
            properties={
                'Container Name': 'test-container',
                'Connection String': connection_string},
            auto_terminate=['success'])
