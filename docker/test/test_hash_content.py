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


def test_hash_invoke():
    """
    Verify sending using InvokeHTTP to a receiver using ListenHTTP.
    """

    invoke_flow = (GetFile('/tmp/input') >> HashAttribute('hash') 
                   >> InvokeHTTP('http://minifi-listen:8080/contentListener', method='POST'))

    listen_flow = ListenHTTP(8080)  >> LogAttribute() >>  PutFile('/tmp/output')

    with DockerTestCluster(SingleFileOutputValidator('test')) as cluster:
        cluster.put_test_data('test')
        cluster.deploy_flow(listen_flow, name='minifi-listen')
        cluster.deploy_flow(invoke_flow, name='minifi-invoke')

        assert cluster.check_output()
