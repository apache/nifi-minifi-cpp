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

import datetime

from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.asymmetric.rsa import RSAPrivateKey
from cryptography.x509 import Certificate, ExtendedKeyUsage
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID


def gen_cert() -> tuple[Certificate, RSAPrivateKey]:
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    subject = issuer = x509.Name(
        [
            x509.NameAttribute(NameOID.COUNTRY_NAME, "US"),
            x509.NameAttribute(NameOID.COMMON_NAME, "minifi-listen"),
        ]
    )

    cert = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(issuer)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.datetime.now(datetime.timezone.utc))
        .not_valid_after(datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=365))
        .sign(key, hashes.SHA256())
    )

    return cert, key


def make_self_signed_cert(common_name: str) -> tuple[Certificate, RSAPrivateKey]:
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    subject = issuer = x509.Name(
        [
            x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        ]
    )

    cert = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(issuer)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.datetime.now(datetime.timezone.utc))
        .not_valid_after(datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=3650))
        .add_extension(x509.SubjectKeyIdentifier.from_public_key(key.public_key()), critical=False)
        .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
        .sign(key, hashes.SHA256())
    )

    return cert, key


def _make_cert(
    common_name: str,
    ca_cert: Certificate,
    ca_key: RSAPrivateKey,
    extended_key_usage: ExtendedKeyUsage | None,
) -> tuple[Certificate, RSAPrivateKey]:
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    subject = x509.Name(
        [
            x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        ]
    )

    builder = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(ca_cert.subject)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(datetime.datetime.now(datetime.timezone.utc))
        .not_valid_after(datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=3650))
        .add_extension(
            x509.BasicConstraints(ca=False, path_length=None),
            critical=True,
        )
        .add_extension(
            x509.SubjectKeyIdentifier.from_public_key(key.public_key()),
            critical=False,
        )
        .add_extension(
            x509.SubjectAlternativeName([x509.DNSName(common_name)]),
            critical=False,
        )
    )

    if extended_key_usage:
        builder = builder.add_extension(x509.ExtendedKeyUsage(extended_key_usage), critical=False)

    cert = builder.sign(ca_key, hashes.SHA256())
    return cert, key


def make_client_cert(common_name: str, ca_cert: Certificate, ca_key: RSAPrivateKey) -> tuple[Certificate, RSAPrivateKey]:
    return _make_cert(
        common_name,
        ca_cert,
        ca_key,
        x509.ExtendedKeyUsage([ExtendedKeyUsageOID.CLIENT_AUTH]),
    )


def make_server_cert(common_name: str, ca_cert: Certificate, ca_key: RSAPrivateKey) -> tuple[Certificate, RSAPrivateKey]:
    return _make_cert(
        common_name,
        ca_cert,
        ca_key,
        x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]),
    )


def make_cert_without_extended_usage(common_name: str, ca_cert: Certificate, ca_key: RSAPrivateKey) -> tuple[Certificate, RSAPrivateKey]:
    return _make_cert(common_name, ca_cert, ca_key, None)


def dump_cert(
    cert: Certificate,
    encoding_type: serialization.Encoding = serialization.Encoding.PEM,
) -> bytes:
    return cert.public_bytes(encoding_type)


def dump_key(
    key: RSAPrivateKey,
    encoding_type: serialization.Encoding = serialization.Encoding.PEM,
) -> bytes:
    return key.private_bytes(
        encoding=encoding_type,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption(),
    )
