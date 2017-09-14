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


def test_minifi_to_nifi():
    """
    Verify sending data from a MiNiFi - C++ to NiFi using S2S protocol.
    """

    port = InputPort('from-minifi', RemoteProcessGroup('http://nifi:8080/nifi'))

    recv_flow = (port
                 >> LogAttribute()
                 >> PutFile('/tmp/output'))

    send_flow = (GetFile('/tmp/input')
                 >> LogAttribute()
                 >> port)

    with DockerTestCluster(SingleFileOutputValidator('test')) as cluster:
        cluster.put_test_data('test')
        cluster.deploy_flow(recv_flow, name='nifi', engine='nifi')
        cluster.deploy_flow(send_flow)

        assert cluster.check_output(60)
