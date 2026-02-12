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

import logging

from OpenSSL import crypto
from minifi_test_framework.core.helpers import wait_for_condition, retry_check
from minifi_test_framework.core.ssl_utils import make_server_cert
from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.file import File
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from docker.errors import ContainerError


class CouchbaseServerContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("couchbase:enterprise-7.2.5", f"couchbase-server-{test_context.scenario_id}", test_context.network)

        couchbase_cert, couchbase_key = make_server_cert(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)

        root_ca_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)
        self.files.append(File("/opt/couchbase/var/lib/couchbase/inbox/CA/root_ca.crt", root_ca_content, permissions=0o666))
        couchbase_cert_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=couchbase_cert)
        self.files.append(File("/opt/couchbase/var/lib/couchbase/inbox/chain.pem", couchbase_cert_content, permissions=0o666))
        couchbase_key_content = crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=couchbase_key)
        self.files.append(File("/opt/couchbase/var/lib/couchbase/inbox/pkey.key", couchbase_key_content, permissions=0o666))

    def deploy(self):
        super().deploy()
        finished_str = "logs available in"
        assert wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=None)
        return self.run_post_startup_commands()

    def run_post_startup_commands(self):
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

        return True

    @retry_check(max_tries=12, retry_interval=5)
    def _run_couchbase_cli_command(self, command):
        (code, output) = self.exec_run(command)
        if code != 0:
            logging.error(f"Failed to run command '{command}', returned error code: {code}, output: '{output}'")
            return False
        return True

    def _run_couchbase_cli_commands(self, commands):
        return all(self._run_couchbase_cli_command(command) for command in commands)

    def _run_python_in_couchbase_helper_docker(self, command: str):
        try:
            self.client.containers.run("minifi-couchbase-helper:latest", ["python", "-c", command], remove=True, stdout=True, stderr=True, network=self.network.name)
            return True
        except ContainerError as e:
            stdout = e.stdout.decode("utf-8", errors="replace") if hasattr(e, "stdout") and e.stdout else ""
            stderr = e.stderr.decode("utf-8", errors="replace") if hasattr(e, "stderr") and e.stderr else ""
            logging.error(f"Python command '{command}' failed in couchbase helper docker with error: '{e}', stdout: '{stdout}', stderr: '{stderr}'")
            return False
        except Exception as e:
            logging.error(f"Unexpected error while running python command '{command}' in couchbase helper docker: '{e}'")
            return False

    @retry_check(max_tries=15, retry_interval=2)
    def _load_couchbase_certs(self):
        python_command = f"""
import requests
import sys
from requests.auth import HTTPBasicAuth
response = requests.post(f"http://{self.container_name}:8091/node/controller/loadTrustedCAs", auth=HTTPBasicAuth("Administrator", "password123"))
if response.status_code != 200:
    sys.exit(1)

response = requests.post(f"http://{self.container_name}:8091/node/controller/reloadCertificate", auth=HTTPBasicAuth("Administrator", "password123"))
if response.status_code != 200:
    sys.exit(1)
sys.exit(0)
        """
        return self._run_python_in_couchbase_helper_docker(python_command)

    def is_data_present_in_couchbase(self, doc_id: str, bucket_name: str, expected_data: str, expected_data_type: str):
        python_command = f"""
from couchbase.cluster import Cluster
from couchbase.options import ClusterOptions
from couchbase.auth import PasswordAuthenticator
from couchbase.transcoder import RawBinaryTranscoder, RawStringTranscoder
import json
import sys

try:
    cluster = Cluster("couchbase://{self.container_name}", ClusterOptions(
        PasswordAuthenticator('Administrator', 'password123')))

    bucket = cluster.bucket("{bucket_name}")
    collection = bucket.default_collection()

    if "{expected_data_type}".lower() == "binary":
        binary_flag = 0x03 << 24
        result = collection.get("{doc_id}", transcoder=RawBinaryTranscoder())
        flags = result.flags
        if not flags & binary_flag:
            print("Expected binary data for document '{doc_id}' but no binary flags were found.")
            sys.exit(1)

        content = result.content_as[bytes]
        if content.decode('utf-8') == '{expected_data}':
            sys.exit(0)

    if "{expected_data_type}".lower() == "json":
        json_flag = 0x02 << 24
        result = collection.get("{doc_id}")
        flags = result.flags
        if not flags & json_flag:
            print("Expected JSON data for document '{doc_id}' but no JSON flags were found.")
            sys.exit(1)

        content = result.content_as[dict]
        if content == json.loads('{expected_data}'):
            sys.exit(0)
    if "{expected_data_type}".lower() == "string":
        string_flag = 0x04 << 24
        result = collection.get("{doc_id}", transcoder=RawStringTranscoder())
        flags = result.flags
        if not flags & string_flag:
            print("Expected string data for document '{doc_id}' but no string flags were found.")
            sys.exit(1)

        content = result.content_as[str]
        if content == '{expected_data}':
            sys.exit(0)

        sys.exit(1)
except Exception as e:
    sys.exit(1)
        """
        return self._run_python_in_couchbase_helper_docker(python_command)
