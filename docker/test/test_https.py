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

import time

from M2Crypto import X509, EVP, RSA, ASN1

from minifi import *

def test_invoke_listen_https_one_way():
    """
    Verify sending using InvokeHTTP to a receiver using ListenHTTP (with TLS).
    """

    cert, key = gen_cert()

    # TODO define SSLContextService class & generate config yml for services
    crt_file = '/tmp/resources/test-crt.pem'

    invoke_flow = (GetFile('/tmp/input')
                   >> InvokeHTTP('https://minifi-listen:4430/contentListener',
                                 method='POST',
                                 ssl_context_service=SSLContextService(cert=crt_file, ca_cert=crt_file)))

    listen_flow = (ListenHTTP(4430, cert=crt_file)
                   >> LogAttribute()
                   >> PutFile('/tmp/output'))

    with DockerTestCluster(SingleFileOutputValidator('test')) as cluster:
        cluster.put_test_resource('test-crt.pem', cert.as_pem() + key.as_pem(None, callback))
        cluster.put_test_data('test')
        cluster.deploy_flow(listen_flow, name='minifi-listen')
        cluster.deploy_flow(invoke_flow, name='minifi-invoke')

        assert cluster.check_output()
