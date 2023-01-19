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
from utils import retry_check


class SplunkChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    @retry_check()
    def check_splunk_event(self, container_name, query):
        (code, output) = self.container_communicator.execute_command(container_name, ["sudo", "/opt/splunk/bin/splunk", "search", query, "-auth", "admin:splunkadmin"])
        if code != 0:
            return False
        return query in output.decode("utf-8")

    @retry_check()
    def check_splunk_event_with_attributes(self, container_name, query, attributes):
        (code, output) = self.container_communicator.execute_command(container_name, ["sudo", "/opt/splunk/bin/splunk", "search", query, "-output", "json", "-auth", "admin:splunkadmin"])
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

    def enable_splunk_hec_indexer(self, container_name, hec_name):
        (code, _) = self.container_communicator.execute_command(container_name, ["sudo",
                                                                                 "/opt/splunk/bin/splunk", "http-event-collector",
                                                                                 "update", hec_name,
                                                                                 "-uri", "https://localhost:8089",
                                                                                 "-use-ack", "1",
                                                                                 "-disabled", "0",
                                                                                 "-auth", "admin:splunkadmin"])
        return code == 0

    def enable_splunk_hec_ssl(self, container_name, splunk_cert_pem, splunk_key_pem, root_ca_cert_pem):
        assert self.container_communicator.write_content_to_container(splunk_cert_pem.decode() + splunk_key_pem.decode() + root_ca_cert_pem.decode(), container_name, '/opt/splunk/etc/auth/splunk_cert.pem')
        assert self.container_communicator.write_content_to_container(root_ca_cert_pem.decode(), container_name, '/opt/splunk/etc/auth/root_ca.pem')
        (code, _) = self.container_communicator.execute_command(container_name, ["sudo",
                                                                                 "/opt/splunk/bin/splunk", "http-event-collector",
                                                                                 "update",
                                                                                 "-uri", "https://localhost:8089",
                                                                                 "-enable-ssl", "1",
                                                                                 "-server-cert", "/opt/splunk/etc/auth/splunk_cert.pem",
                                                                                 "-ca-cert-file", "/opt/splunk/etc/auth/root_ca.pem",
                                                                                 "-require-client-cert", "1",
                                                                                 "-auth", "admin:splunkadmin"])
        return code == 0
