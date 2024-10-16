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
import OpenSSL.crypto
import tempfile
import docker
import requests
import logging
from requests.auth import HTTPBasicAuth
from .Container import Container
from utils import retry_check
from ssl_utils.SSL_cert_utils import make_server_cert


class CouchbaseServerContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None, ssl=False):
        self.ssl = ssl
        engine = "couchbase-server" if not ssl else "couchbase-server-ssl"
        super().__init__(feature_context, name, engine, vols, network, image_store, command)
        couchbase_cert, couchbase_key = make_server_cert(f"couchbase-server-{feature_context.id}", feature_context.root_ca_cert, feature_context.root_ca_key)

        self.root_ca_file = tempfile.NamedTemporaryFile(delete=False)
        self.root_ca_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=feature_context.root_ca_cert))
        self.root_ca_file.close()
        os.chmod(self.root_ca_file.name, 0o666)

        self.couchbase_cert_file = tempfile.NamedTemporaryFile(delete=False)
        self.couchbase_cert_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=couchbase_cert))
        self.couchbase_cert_file.close()
        os.chmod(self.couchbase_cert_file.name, 0o666)

        self.couchbase_key_file = tempfile.NamedTemporaryFile(delete=False)
        self.couchbase_key_file.write(OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM, pkey=couchbase_key))
        self.couchbase_key_file.close()
        os.chmod(self.couchbase_key_file.name, 0o666)

    def get_startup_finished_log_entry(self):
        # after startup the logs are only available in the container, only this message is shown
        return "logs available in"

    @retry_check(12, 5)
    def _run_couchbase_cli_command(self, command):
        (code, _) = self.client.containers.get(self.name).exec_run(command)
        if code != 0:
            logging.error(f"Failed to run command '{command}', returned error code: {code}")
            return False
        return True

    def _run_couchbase_cli_commands(self, commands):
        for command in commands:
            if not self._run_couchbase_cli_command(command):
                return False
        return True

    @retry_check(15, 2)
    def _load_couchbase_certs(self):
        response = requests.post("http://localhost:8091/node/controller/loadTrustedCAs", auth=HTTPBasicAuth("Administrator", "password123"))
        if response.status_code != 200:
            logging.error(f"Failed to load CA certificates, with status code: {response.status_code}")
            return False

        response = requests.post("http://localhost:8091/node/controller/reloadCertificate", auth=HTTPBasicAuth("Administrator", "password123"))
        if response.status_code != 200:
            logging.error(f"Failed to reload certificates, with status code: {response.status_code}")
            return False

        return True

    def run_post_startup_commands(self):
        if self.post_startup_commands_finished:
            return True

        commands = [
            ["couchbase-cli", "cluster-init", "-c", "localhost", "--cluster-username", "Administrator", "--cluster-password", "password123", "--services", "data,index,query",
             "--cluster-ramsize", "2048", "--cluster-index-ramsize", "256"],
            ["couchbase-cli", "bucket-create", "-c", "localhost", "--username", "Administrator", "--password", "password123", "--bucket", "test_bucket", "--bucket-type", "couchbase",
             "--bucket-ramsize", "1024", "--max-ttl", "36000"],
            ["couchbase-cli", "user-manage", "-c", "localhost", "-u", "Administrator", "-p", "password123", "--set", "--rbac-username", "clientuser", "--rbac-password", "password123",
             "--roles", "data_reader[test_bucket],data_writer[test_bucket]", "--auth-domain", "local"],
            ["bash", "-c", 'tee /tmp/auth.json <<< \'{"state": "enable", "prefixes": [ {"path": "subject.cn", "prefix": "", "delimiter": "."}]}\''],
            ['couchbase-cli', 'ssl-manage', '-c', 'localhost', '-u', 'Administrator', '-p', 'password123', '--set-client-auth', '/tmp/auth.json']
        ]
        if not self._run_couchbase_cli_commands(commands):
            return False

        if not self._load_couchbase_certs():
            return False

        self.post_startup_commands_finished = True
        return True

    def deploy(self):
        if not self.set_deployed():
            return

        mounts = [
            docker.types.Mount(
                type='bind',
                source=self.couchbase_key_file.name,
                target='/opt/couchbase/var/lib/couchbase/inbox/pkey.key'),
            docker.types.Mount(
                type='bind',
                source=self.couchbase_cert_file.name,
                target='/opt/couchbase/var/lib/couchbase/inbox/chain.pem'),
            docker.types.Mount(
                type='bind',
                source=self.root_ca_file.name,
                target='/opt/couchbase/var/lib/couchbase/inbox/CA/root_ca.crt')
        ]

        self.docker_container = self.client.containers.run(
            "couchbase:enterprise-7.2.5",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'8091/tcp': 8091, '11210/tcp': 11210},
            entrypoint=self.command,
            mounts=mounts)
