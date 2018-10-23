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
from minifi.test import *


def callback():
    pass


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


def gen_cert():
    """
    Generate TLS certificate request for testing
    """

    req, key = gen_req()
    pub_key = req.get_pubkey()
    subject = req.get_subject()
    cert = X509.X509()
    # noinspection PyTypeChecker
    cert.set_serial_number(1)
    cert.set_version(2)
    cert.set_subject(subject)
    t = int(time.time())
    now = ASN1.ASN1_UTCTIME()
    now.set_time(t)
    now_plus_year = ASN1.ASN1_UTCTIME()
    now_plus_year.set_time(t + 60 * 60 * 24 * 365)
    cert.set_not_before(now)
    cert.set_not_after(now_plus_year)
    issuer = X509.X509_Name()
    issuer.C = 'US'
    issuer.CN = 'minifi-listen'
    cert.set_issuer(issuer)
    cert.set_pubkey(pub_key)
    cert.sign(key, 'sha256')

    return cert, key


def gen_req():
    """
    Generate TLS certificate request for testing
    """

    logging.info('Generating test certificate request')
    key = EVP.PKey()
    req = X509.Request()
    rsa = RSA.gen_key(1024, 65537, callback)
    key.assign_rsa(rsa)
    req.set_pubkey(key)
    name = req.get_subject()
    name.C = 'US'
    name.CN = 'minifi-listen'
    req.sign(key, 'sha256')

    return req, key
