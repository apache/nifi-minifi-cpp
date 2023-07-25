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


import time
import logging
import random

from M2Crypto import X509, EVP, RSA, ASN1
from OpenSSL import crypto


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


def rsa_gen_key_callback():
    pass


def gen_req():
    """
    Generate TLS certificate request for testing
    """

    logging.info('Generating test certificate request')
    key = EVP.PKey()
    req = X509.Request()
    rsa = RSA.gen_key(1024, 65537, rsa_gen_key_callback)
    key.assign_rsa(rsa)
    req.set_pubkey(key)
    name = req.get_subject()
    name.C = 'US'
    name.CN = 'minifi-listen'
    req.sign(key, 'sha256')

    return req, key


def make_self_signed_cert(common_name):
    ca_key = crypto.PKey()
    ca_key.generate_key(crypto.TYPE_RSA, 2048)

    ca_cert = crypto.X509()
    ca_cert.set_version(2)
    ca_cert.set_serial_number(random.randint(50000000, 100000000))

    ca_subj = ca_cert.get_subject()
    ca_subj.commonName = common_name

    ca_cert.add_extensions([
        crypto.X509Extension(b"subjectKeyIdentifier", False, b"hash", subject=ca_cert),
    ])

    ca_cert.add_extensions([
        crypto.X509Extension(b"authorityKeyIdentifier", False, b"keyid:always", issuer=ca_cert),
    ])

    ca_cert.add_extensions([
        crypto.X509Extension(b"basicConstraints", False, b"CA:TRUE"),
        crypto.X509Extension(b"keyUsage", False, b"keyCertSign, cRLSign"),
    ])

    ca_cert.set_issuer(ca_subj)
    ca_cert.set_pubkey(ca_key)

    ca_cert.gmtime_adj_notBefore(0)
    ca_cert.gmtime_adj_notAfter(10 * 365 * 24 * 60 * 60)

    ca_cert.sign(ca_key, 'sha256')

    return ca_cert, ca_key


def _make_cert(common_name, ca_cert, ca_key, extended_key_usage=None):
    key = crypto.PKey()
    key.generate_key(crypto.TYPE_RSA, 2048)

    cert = crypto.X509()
    cert.set_version(2)
    cert.set_serial_number(random.randint(50000000, 100000000))

    client_subj = cert.get_subject()
    client_subj.commonName = common_name

    cert.add_extensions([
        crypto.X509Extension(b"basicConstraints", False, b"CA:FALSE"),
        crypto.X509Extension(b"subjectKeyIdentifier", False, b"hash", subject=cert),
    ])

    extensions = [crypto.X509Extension(b"authorityKeyIdentifier", False, b"keyid:always", issuer=ca_cert),
                  crypto.X509Extension(b"keyUsage", False, b"digitalSignature")]

    if extended_key_usage:
        extensions.append(crypto.X509Extension(b"extendedKeyUsage", False, extended_key_usage))

    cert.add_extensions([
        crypto.X509Extension(b"subjectAltName", False, b"DNS.1:" + common_name.encode())
    ])

    cert.add_extensions(extensions)

    cert.set_issuer(ca_cert.get_subject())
    cert.set_pubkey(key)

    cert.gmtime_adj_notBefore(0)
    cert.gmtime_adj_notAfter(10 * 365 * 24 * 60 * 60)

    cert.sign(ca_key, 'sha256')

    return cert, key


def make_client_cert(common_name, ca_cert, ca_key):
    return _make_cert(common_name=common_name, ca_cert=ca_cert, ca_key=ca_key, extended_key_usage=b"clientAuth")


def make_server_cert(common_name, ca_cert, ca_key):
    return _make_cert(common_name=common_name, ca_cert=ca_cert, ca_key=ca_key, extended_key_usage=b"serverAuth")


def make_cert_without_extended_usage(common_name, ca_cert, ca_key):
    return _make_cert(common_name=common_name, ca_cert=ca_cert, ca_key=ca_key, extended_key_usage=None)
