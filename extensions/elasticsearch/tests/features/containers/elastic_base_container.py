# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from minifi_behave.containers.container_linux import LinuxContainer
from minifi_behave.core.helpers import retry_check, wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


class ElasticBaseContainer(LinuxContainer):
    def __init__(self, test_context: MinifiTestContext, image: str, container_name: str):
        super().__init__(image, container_name, test_context.network)
        self.user = None

    def deploy(self, context: MinifiTestContext | None, finished_str: str):
        super().deploy(context)
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=300,
            bail_condition=lambda: self.exited,
            context=context,
        )

    def create_doc_elasticsearch(self, index_name: str, doc_id: str) -> bool:
        (code, output) = self.exec_run(
            [
                "/bin/bash",
                "-c",
                "curl -s -u elastic:password -k -XPUT https://localhost:9200/"
                + index_name
                + "/_doc/"
                + doc_id
                + ' -H Content-Type:application/json -d\'{"field1":"value1"}\'',
            ]
        )
        return code == 0 and ('"_id":"' + doc_id + '"') in output

    def check_elastic_field_value(self, index_name: str, doc_id: str, field_name: str, field_value: str) -> bool:
        (code, output) = self.exec_run(
            [
                "/bin/bash",
                "-c",
                "curl -s -u elastic:password -k -XGET https://localhost:9200/" + index_name + "/_doc/" + doc_id,
            ]
        )
        return code == 0 and (field_name + '":"' + field_value) in output

    @retry_check()
    def is_elasticsearch_empty(self) -> bool:
        (code, output) = self.exec_run(
            [
                "curl",
                "-s",
                "-u",
                "elastic:password",
                "-k",
                "-XGET",
                "https://localhost:9200/_search",
            ]
        )
        return code == 0 and '"hits":[]' in output
