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
from minifi_test_framework.core.helpers import wait_for_condition, retry_check
from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class FakeGcsServerContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("fsouza/fake-gcs-server:1.45.1", f"fake-gcs-server-{test_context.scenario_id}", test_context.network,
                         command=f'-scheme http -host fake-gcs-server-{test_context.scenario_id}')
        self.dirs.append(Directory(path="/data/test-bucket", files={"test-file": "preloaded data\n"}))

    def deploy(self):
        super().deploy()
        finished_str = "server started at http"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=30,
            bail_condition=lambda: self.exited,
            context=None)

    @retry_check()
    def check_google_cloud_storage(self, content):
        (code, output) = self.exec_run(["grep", "-r", content, "/storage"])
        logging.info(f"GCS storage contents matching '{content}': {output}")
        return code == 0

    @retry_check()
    def is_gcs_bucket_empty(self):
        (code, output) = self.exec_run(["ls", "/storage/test-bucket"])
        logging.info(f"GCS bucket contents: {output}")
        return code == 0 and output == ""
