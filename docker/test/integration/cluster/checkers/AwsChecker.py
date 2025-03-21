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


class AwsChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    @retry_check()
    def check_s3_server_object_data(self, container_name, test_data):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        if code != 0:
            return False
        s3_mock_dir = output.strip()
        (code, file_data) = self.container_communicator.execute_command(container_name, ["cat", s3_mock_dir + "/binaryData"])
        return code == 0 and file_data == test_data

    @retry_check()
    def check_s3_server_object_hash(self, container_name: str, expected_file_hash: str):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        if code != 0:
            return False
        dir_candidates = output.split("\n")
        for candidate in dir_candidates:
            if "multiparts" not in candidate:
                s3_mock_dir = candidate
                break
        (code, md5_output) = self.container_communicator.execute_command(container_name, ["md5sum", s3_mock_dir + "/binaryData"])
        if code != 0:
            return False
        file_hash = md5_output.split(' ')[0].strip()
        return file_hash == expected_file_hash

    @retry_check()
    def check_s3_server_object_metadata(self, container_name, content_type="application/octet-stream", metadata=dict()):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        if code != 0:
            return False
        s3_mock_dir = output.strip()
        (code, output) = self.container_communicator.execute_command(container_name, ["cat", s3_mock_dir + "/objectMetadata.json"])
        server_metadata = json.loads(output)
        return code == 0 and server_metadata["contentType"] == content_type and metadata == server_metadata["userMetadata"]

    @retry_check()
    def is_s3_bucket_empty(self, container_name):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/s3mockroot/test_bucket", "-mindepth", "1", "-maxdepth", "1", "-type", "d"])
        return code == 0 and not output.strip()
