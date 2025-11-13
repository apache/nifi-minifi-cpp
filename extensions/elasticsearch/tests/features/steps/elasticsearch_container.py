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

import json
import os

from elastic_base_container import ElasticBaseContainer
from pathlib import Path
from OpenSSL import crypto
from minifi_test_framework.core.ssl_utils import make_server_cert, make_cert_without_extended_usage
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.host_file import HostFile
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class ElasticsearchContainer(ElasticBaseContainer):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__(test_context, "elasticsearch:9.1.5", f"elasticsearch-{test_context.scenario_id}")

        http_cert, http_key = make_server_cert(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)
        transport_cert, transport_key = make_cert_without_extended_usage(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)

        root_ca_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)
        self.files.append(File("/usr/share/elasticsearch/config/certs/", "root_ca.crt", root_ca_content, permissions=0o644))

        http_cert_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=http_cert)
        self.files.append(File("/usr/share/elasticsearch/config/certs/", "elastic_http.crt", http_cert_content, permissions=0o644))

        http_key_content = crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=http_key)
        self.files.append(File("/usr/share/elasticsearch/config/certs/", "elastic_http.key", http_key_content, permissions=0o644))

        transport_cert_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=transport_cert)
        self.files.append(File("/usr/share/elasticsearch/config/certs/", "elastic_transport.crt", transport_cert_content, permissions=0o644))

        transport_key_content = crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=transport_key)
        self.files.append(File("/usr/share/elasticsearch/config/certs/", "elastic_transport.key", transport_key_content, permissions=0o644))

        features_dir = Path(__file__).resolve().parent.parent
        self.host_files.append(HostFile('/usr/share/elasticsearch/config/elasticsearch.yml', os.path.join(features_dir, "resources", "elasticsearch.yml")))

        self.environment.append("ELASTIC_PASSWORD=password")

    def deploy(self):
        return super().deploy('"current.health":"GREEN"')

    def elastic_generate_apikey(self):
        (code, output) = self.exec_run(["/bin/bash", "-c",
                                        "curl -s -u elastic:password -k -XPOST https://localhost:9200/_security/api_key -H Content-Type:application/json -d'{\"name\":\"my-api-key\",\"expiration\":\"1d\",\"role_descriptors\":{\"role-a\": {\"cluster\": [\"all\"],\"index\": [{\"names\": [\"my_index\"],\"privileges\": [\"all\"]}]}}}'"])
        if code != 0:
            return None
        output_lines = output.splitlines()
        result = json.loads(output_lines[-1])
        return result["encoded"]
