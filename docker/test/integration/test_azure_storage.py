# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the \"License\"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an \"AS IS\" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from minifi import *
from minifi.test import *


def test_blob_upload():
    """
    Verify blob upload to Azure storage
    """
    flow = (GetFile('/tmp/input') >> PutAzureBlobStorage() \
                 >> LogAttribute() \
                 >> PutFile('/tmp/output/success'))

    with DockerTestCluster(SingleFileOutputValidator('test', subdir='success')) as cluster:
        cluster.put_test_data('#test_data$123$#')
        cluster.deploy_flow(None, engine='azure-storage-server')
        cluster.deploy_flow(flow, engine='minifi-cpp', name='minifi-cpp')

        assert cluster.check_output(60)
        assert cluster.check_azure_storage_server_data()
