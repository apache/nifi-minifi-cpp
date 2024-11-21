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
import requests
from typing import List
from utils import wait_for


class GrafanaLokiChecker:
    def __init__(self):
        self.url = "localhost:3100/loki/api/v1/query_range"

    def veify_log_lines_on_grafana_loki(self, lines: List[str], ssl: bool, tenant_id: str):
        labels = '{job="minifi"}'
        prefix = "http://"
        if ssl:
            prefix = "https://"

        query_url = f"{prefix}{self.url}?query={labels}"

        headers = None
        if tenant_id:
            headers = {'X-Scope-OrgID': tenant_id}

        response = requests.get(query_url, verify=False, timeout=30, headers=headers)
        if response.status_code < 200 or response.status_code >= 300:
            return False

        json_response = response.json()
        if "data" not in json_response or "result" not in json_response["data"] or len(json_response["data"]["result"]) < 1:
            return False

        result = json_response["data"]["result"][0]
        if "values" not in result:
            return False

        for line in lines:
            if line not in str(result["values"]):
                return False
        return True

    def wait_for_lines_on_grafana_loki(self, lines: List[str], timeout_seconds: int, ssl: bool, tenant_id: str):
        return wait_for(lambda: self.veify_log_lines_on_grafana_loki(lines, ssl, tenant_id), timeout_seconds)
