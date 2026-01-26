#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

import jks
import os
from OpenSSL import crypto
from cryptography.hazmat.primitives.serialization import pkcs12, BestAvailableEncryption, load_pem_private_key
from cryptography import x509
from pathlib import Path

from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.ssl_utils import make_server_cert
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.host_file import HostFile


class MinifiC2Server(Container):
    def __init__(self, test_context: MinifiTestContext, ssl: bool = False):
        super().__init__("apache/nifi-minifi-c2:1.27.0", f"minifi-c2-server-{test_context.scenario_id}", test_context.network)
        if ssl:
            c2_cert, c2_key = make_server_cert(f"minifi-c2-server-{test_context.scenario_id}", test_context.root_ca_cert, test_context.root_ca_key)
            pke = jks.PrivateKeyEntry.new('c2-server-cert', [crypto.dump_certificate(crypto.FILETYPE_ASN1, c2_cert)], crypto.dump_privatekey(crypto.FILETYPE_ASN1, c2_key), 'rsa_raw')
            server_keystore = jks.KeyStore.new('jks', [pke])
            server_keystore_content = server_keystore.saves('abcdefgh')
            self.files.append(File("/opt/minifi-c2/minifi-c2-current/certs/minifi-c2-server-keystore.jks", server_keystore_content, permissions=0o644))

            private_key_pem = crypto.dump_privatekey(crypto.FILETYPE_PEM, test_context.root_ca_key)
            private_key = load_pem_private_key(private_key_pem, password=None)
            certificate_pem = crypto.dump_certificate(crypto.FILETYPE_PEM, test_context.root_ca_cert)
            certificate = x509.load_pem_x509_certificate(certificate_pem)
            pkcs12_data = pkcs12.serialize_key_and_certificates(
                name=None,
                key=private_key,
                cert=certificate,
                cas=None,
                encryption_algorithm=BestAvailableEncryption(b'abcdefgh')
            )
            self.files.append(File("/opt/minifi-c2/minifi-c2-current/certs/minifi-c2-server-truststore.p12", pkcs12_data, permissions=0o644))

            authorities_file_content = """
  CN=minifi-primary-{scenario_id}:
  - CLASS_MINIFI_CPP
  """.format(scenario_id=test_context.scenario_id)
            self.files.append(File("/opt/minifi-c2/minifi-c2-current/conf/authorities.yaml", authorities_file_content, permissions=0o644))

        resource_dir = Path(__file__).resolve().parent / "resources" / "minifi-c2-server"
        self.host_files.append(HostFile("/opt/minifi-c2/minifi-c2-current/files/minifi-test-class/config.text.yml.v1", os.path.join(resource_dir, "config.yml")))
        if ssl:
            self.host_files.append(HostFile("/opt/minifi-c2/minifi-c2-current/conf/authorizations.yaml", os.path.join(resource_dir, "authorizations.yaml")))
            self.host_files.append(HostFile("/opt/minifi-c2/minifi-c2-current/conf/c2.properties", os.path.join(resource_dir, "c2.properties")))

    def deploy(self):
        super().deploy()
        finished_str = "Server Started"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=60,
            bail_condition=lambda: self.exited,
            context=None
        )
