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

from minifi_test_framework.core.helpers import wait_for_condition, retry_check
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class ElasticBaseContainer(Container):
    def __init__(self, test_context: MinifiTestContext, image: str, container_name: str):
        super().__init__(image, container_name, test_context.network)
        self.user = None

    def deploy(self, finished_str: str):
        super().deploy()
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=300,
            bail_condition=lambda: self.exited,
            context=None)

    def create_doc_elasticsearch(self, index_name: str, doc_id: str) -> bool:
        (code, output) = self.exec_run(["/bin/bash", "-c",
                                        "curl -s -u elastic:password -k -XPUT https://localhost:9200/" + index_name + "/_doc/"
                                        + doc_id + " -H Content-Type:application/json -d'{\"field1\":\"value1\"}'"])
        return code == 0 and ('"_id":"' + doc_id + '"') in output

    def check_elastic_field_value(self, index_name: str, doc_id: str, field_name: str, field_value: str) -> bool:
        (code, output) = self.exec_run(["/bin/bash", "-c", "curl -s -u elastic:password -k -XGET https://localhost:9200/" + index_name + "/_doc/" + doc_id])
        return code == 0 and (field_name + '":"' + field_value) in output

    @retry_check()
    def is_elasticsearch_empty(self) -> bool:
        (code, output) = self.exec_run(["curl", "-s", "-u", "elastic:password", "-k", "-XGET", "https://localhost:9200/_search"])
        return code == 0 and '"hits":[]' in output
