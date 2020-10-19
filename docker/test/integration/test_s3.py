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


def test_put_s3_object():
    """
    Verify delivery of S3 object to AWS server
    """
    flow = (GetFile('/tmp/input') >> PutS3Object() \
                 >> LogAttribute() \
                 >> PutFile('/tmp/output/success'))

    with DockerTestCluster(SingleFileOutputValidator('test', subdir='success')) as cluster:
        cluster.put_test_data('LH_O#L|FD<FASD{FO#@$#$%^ "#"$L%:"@#$L":test_data#$#%#$%?{"F{')
        cluster.deploy_flow(None, engine='s3-server')
        cluster.deploy_flow(flow, engine='minifi-cpp', name='minifi-cpp')

        assert cluster.check_output(60)

        assert cluster.check_s3_server_object_data()
        assert cluster.check_s3_server_object_metadata()

def test_put_s3_object_proxy():
    """
    Verify delivery of S3 object to AWS server through proxy server
    """
    flow = (GetFile('/tmp/input') \
            >> PutS3Object(proxy_host='http-proxy',
                           proxy_port='3128',
                           proxy_username='admin',
                           proxy_password='test101') \
            >> LogAttribute() \
            >> PutFile('/tmp/output/success'))

    with DockerTestCluster(SingleFileOutputValidator('test', subdir='success')) as cluster:
        cluster.put_test_data('LH_O#L|FD<FASD{FO#@$#$%^ "#"$L%:"@#$L":test_data#$#%#$%?{"F{')
        cluster.deploy_flow(None, engine='http-proxy')
        cluster.deploy_flow(None, engine='s3-server')
        cluster.deploy_flow(flow, engine='minifi-cpp', name='minifi-cpp')

        assert cluster.check_output(60)

        assert cluster.check_http_proxy_access("http://s3-server:9090/test_bucket/test_object_key")
        assert cluster.check_s3_server_object_data()
        assert cluster.check_s3_server_object_metadata()

def test_delete_s3_object():
    """
    Verify deletion of S3 object
    """
    flow = (GetFile('/tmp/input') >> PutS3Object() \
            >> LogAttribute() \
            >> DeleteS3Object() \
            >> PutFile('/tmp/output/success'))

    with DockerTestCluster(SingleFileOutputValidator('test', subdir='success')) as cluster:
        cluster.put_test_data('test_data')
        cluster.deploy_flow(None, engine='s3-server')
        cluster.deploy_flow(flow, engine='minifi-cpp', name='minifi-cpp')
        assert cluster.check_output(60)
        assert cluster.is_s3_bucket_empty()

def test_delete_s3_object_proxy():
    """
    Verify deletion of S3 object through proxy server
    """
    flow = (GetFile('/tmp/input') >> PutS3Object() \
            >> LogAttribute() \
            >> DeleteS3Object(proxy_host='http-proxy',
                              proxy_port='3128',
                              proxy_username='admin',
                              proxy_password='test101') \
            >> PutFile('/tmp/output/success'))

    with DockerTestCluster(SingleFileOutputValidator('test', subdir='success')) as cluster:
        cluster.put_test_data('test_data')
        cluster.deploy_flow(None, engine='s3-server')
        cluster.deploy_flow(None, engine='http-proxy')
        cluster.deploy_flow(flow, engine='minifi-cpp', name='minifi-cpp')
        assert cluster.check_output(60)
        assert cluster.is_s3_bucket_empty()
        assert cluster.check_http_proxy_access("http://s3-server:9090/test_bucket/test_object_key")
