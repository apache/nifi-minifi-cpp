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

from OpenSSL import crypto
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.helpers import wait_for_condition, retry_check
from minifi_test_framework.containers.file import File
from minifi_test_framework.core.ssl_utils import make_server_cert


class SplunkContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("splunk/splunk:9.2.1-patch2", f"splunk-{test_context.scenario_id}", test_context.network)
        self.user = None

        self.environment = ["SPLUNK_LICENSE_URI=Free",
                            "SPLUNK_START_ARGS=--accept-license",
                            "SPLUNK_PASSWORD=splunkadmin"]

        splunk_config_content = """
splunk:
  hec:
    enable: True
    ssl: False
    port: 8088
    token: 176fae97-f59d-4f08-939a-aa6a543f2485
"""
        self.files.append(File("/tmp/defaults/default.yml", splunk_config_content, mode="rw", permissions=0o644))

        splunk_cert, splunk_key = make_server_cert(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)
        splunk_cert_content = crypto.dump_certificate(crypto.FILETYPE_PEM, splunk_cert)
        splunk_key_content = crypto.dump_privatekey(crypto.FILETYPE_PEM, splunk_key)
        root_ca_content = crypto.dump_certificate(crypto.FILETYPE_PEM, test_context.root_ca_cert)
        self.files.append(File("/opt/splunk/etc/auth/splunk_cert.pem", splunk_cert_content.decode() + splunk_key_content.decode() + root_ca_content.decode(), permissions=0o644))
        self.files.append(File("/opt/splunk/etc/auth/root_ca.pem", root_ca_content.decode(), permissions=0o644))

    def deploy(self):
        super().deploy()
        finished_str = "Ansible playbook complete, will begin streaming splunkd_stderr.log"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=300,
            bail_condition=lambda: False,
            context=None)

    @retry_check()
    def check_splunk_event(self, query: str) -> bool:
        (code, output) = self.exec_run(["sudo", "/opt/splunk/bin/splunk", "search", query, "-auth", "admin:splunkadmin"])
        if code != 0:
            return False
        return query in output.decode("utf-8")

    @retry_check()
    def check_splunk_event_with_attributes(self, query: str, attributes: dict[str, str]) -> bool:
        (code, output) = self.exec_run(["sudo", "/opt/splunk/bin/splunk", "search", query, "-output", "json", "-auth", "admin:splunkadmin"])
        if code != 0:
            return False
        result_lines = output.splitlines()
        for result_line in result_lines:
            try:
                result_line_json = json.loads(result_line)
            except json.decoder.JSONDecodeError:
                continue
            if "result" not in result_line_json:
                continue
            if "host" in attributes:
                if result_line_json["result"]["host"] != attributes["host"]:
                    continue
            if "source" in attributes:
                if result_line_json["result"]["source"] != attributes["source"]:
                    continue
            if "sourcetype" in attributes:
                if result_line_json["result"]["sourcetype"] != attributes["sourcetype"]:
                    continue
            if "index" in attributes:
                if result_line_json["result"]["index"] != attributes["index"]:
                    continue
            return True
        return False

    def enable_splunk_hec_indexer(self, hec_name: str):
        (code, _) = self.exec_run(["sudo",
                                   "/opt/splunk/bin/splunk", "http-event-collector",
                                   "update", hec_name,
                                   "-uri", "https://localhost:8089",
                                   "-use-ack", "1",
                                   "-disabled", "0",
                                   "-auth", "admin:splunkadmin"])
        return code == 0

    def enable_splunk_hec_ssl(self):
        (code, _) = self.exec_run(["sudo",
                                   "/opt/splunk/bin/splunk", "http-event-collector",
                                   "update",
                                   "-uri", "https://localhost:8089",
                                   "-enable-ssl", "1",
                                   "-server-cert", "/opt/splunk/etc/auth/splunk_cert.pem",
                                   "-ca-cert-file", "/opt/splunk/etc/auth/root_ca.pem",
                                   "-require-client-cert", "1",
                                   "-auth", "admin:splunkadmin"])
        return code == 0
