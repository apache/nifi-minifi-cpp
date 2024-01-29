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
import os
import tempfile

import docker.types
import jks

from .Container import Container
from OpenSSL import crypto
from ssl_utils.SSL_cert_utils import make_server_cert


class MinifiC2ServerContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None, ssl=False):
        engine = "minifi-c2-server-ssl" if ssl else "minifi-c2-server"
        super().__init__(feature_context, name, engine, vols, network, image_store, command)
        self.ssl = ssl
        if ssl:
            c2_cert, c2_key = make_server_cert(f"minifi-c2-server-{feature_context.id}", feature_context.root_ca_cert, feature_context.root_ca_key)
            pke = jks.PrivateKeyEntry.new('c2-server-cert', [crypto.dump_certificate(crypto.FILETYPE_ASN1, c2_cert)], crypto.dump_privatekey(crypto.FILETYPE_ASN1, c2_key), 'rsa_raw')
            server_keystore = jks.KeyStore.new('jks', [pke])
            self.server_keystore_file = tempfile.NamedTemporaryFile(delete=False)
            server_keystore.save(self.server_keystore_file.name, 'abcdefgh')
            os.chmod(self.server_keystore_file.name, 0o644)

            self.server_truststore_file = tempfile.NamedTemporaryFile(delete=False)
            pkcs12 = crypto.PKCS12()
            pkcs12.set_privatekey(feature_context.root_ca_key)
            pkcs12.set_certificate(feature_context.root_ca_cert)
            pkcs12_data = pkcs12.export(passphrase="abcdefgh")
            self.server_truststore_file.write(pkcs12_data)
            self.server_truststore_file.close()
            os.chmod(self.server_truststore_file.name, 0o644)

            self.authorities_file = tempfile.NamedTemporaryFile(delete=False)
            authorities_file_content = """
CN=minifi-cpp-flow-{feature_id}:
  - CLASS_MINIFI_CPP
""".format(feature_id=self.feature_context.id)
            self.authorities_file.write(authorities_file_content.encode())
            self.authorities_file.close()
            os.chmod(self.authorities_file.name, 0o644)

    def get_startup_finished_log_entry(self):
        return "Server Started"

    def deploy(self):
        if not self.set_deployed():
            return

        test_dir = os.environ['TEST_DIRECTORY']  # Based on DockerVerify.sh
        mounts = [docker.types.Mount(
            type='bind',
            source=os.path.join(test_dir, "resources/minifi-c2-server/config-ssl.json" if self.ssl else "resources/minifi-c2-server/config.json"),
            target='/opt/minifi-c2/minifi-c2-current/files/minifi-test-class/config.application.json.v1'
        )]

        if self.ssl:
            mounts.append(docker.types.Mount(
                type='bind',
                source=self.authorities_file.name,
                target='/opt/minifi-c2/minifi-c2-current/conf/authorities.yaml'
            ))
            mounts.append(docker.types.Mount(
                type='bind',
                source=os.path.join(test_dir, "resources/minifi-c2-server/authorizations.yaml"),
                target='/opt/minifi-c2/minifi-c2-current/conf/authorizations.yaml'
            ))
            mounts.append(docker.types.Mount(
                type='bind',
                source=os.path.join(test_dir, "resources/minifi-c2-server/c2.properties"),
                target='/opt/minifi-c2/minifi-c2-current/conf/c2.properties'
            ))
            mounts.append(docker.types.Mount(
                type='bind',
                source=self.server_keystore_file.name,
                target='/opt/minifi-c2/minifi-c2-current/certs/minifi-c2-server-keystore.jks'
            ))
            mounts.append(docker.types.Mount(
                type='bind',
                source=self.server_truststore_file.name,
                target='/opt/minifi-c2/minifi-c2-current/certs/minifi-c2-server-truststore.p12'
            ))
        self.docker_container = self.client.containers.run(
            image='apache/nifi-minifi-c2:1.25.0',
            detach=True,
            name=self.name,
            network=self.network.name,
            mounts=mounts,
            entrypoint=self.command)
