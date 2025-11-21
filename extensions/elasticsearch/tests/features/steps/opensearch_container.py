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
import logging

from elastic_base_container import ElasticBaseContainer
from pathlib import Path
from OpenSSL import crypto
from minifi_test_framework.core.ssl_utils import make_server_cert
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.host_file import HostFile
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class OpensearchContainer(ElasticBaseContainer):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__(test_context, "opensearchproject/opensearch:2.6.0", f"opensearch-{test_context.scenario_id}")

        admin_pem, admin_key = make_server_cert(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)

        root_ca_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)
        self.files.append(File("/usr/share/opensearch/config/root-ca.pem", root_ca_content, permissions=0o644))

        admin_pem_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=admin_pem)
        self.files.append(File("/usr/share/opensearch/config/admin.pem", admin_pem_content, permissions=0o644))

        admin_key_content = crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=admin_key)
        self.files.append(File("/usr/share/opensearch/config/admin-key.pem", admin_key_content, permissions=0o644))

        features_dir = Path(__file__).resolve().parent.parent
        self.host_files.append(HostFile('/usr/share/opensearch/config/opensearch.yml', os.path.join(features_dir, "resources", "opensearch.yml")))

    def deploy(self):
        return super().deploy('Hot-reloading of audit configuration is enabled')

    def add_elastic_user_to_opensearch(self):
        curl_cmd = [
            "curl -s",
            "-u admin:admin",
            "-k",
            "-XPUT",
            f"https://{self.container_name}:9200/_plugins/_security/api/internalusers/elastic",
            "-H 'Content-Type:application/json'",
            "-d '{\"password\":\"password\",\"backend_roles\":[\"admin\"]}'"
        ]
        full_cmd = " ".join(curl_cmd)
        (code, output) = self.exec_run(["/bin/bash", "-c", full_cmd])
        logging.info(f"Add elastic user to Opensearch output: {output}")
        return code == 0 and '"status":"CREATED"' in output
