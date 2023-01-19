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
from utils import retry_check


class GcsChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    @retry_check()
    def check_google_cloud_storage(self, gcs_container_name, content):
        (code, _) = self.container_communicator.execute_command(gcs_container_name, ["grep", "-r", content, "/storage"])
        return code == 0

    @retry_check()
    def is_gcs_bucket_empty(self, container_name):
        (code, output) = self.container_communicator.execute_command(container_name, ["ls", "/storage/test-bucket"])
        return code == 0 and output == ""
