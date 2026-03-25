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

from typing import Protocol, List

from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.host_file import HostFile


class ContainerProtocol(Protocol):
    image_name: str
    container_name: str
    dirs: List[Directory]
    files: List[File]
    host_files: List[HostFile]

    def deploy(self, context) -> bool:
        ...

    def clean_up(self):
        ...

    def exec_run(self, command):
        ...

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        ...

    def directory_contains_file_with_regex(self, directory_path: str, regex_str: str) -> bool:
        ...

    def path_with_content_exists(self, path: str, content: str) -> bool:
        ...

    def get_logs(self) -> str:
        ...

    @property
    def exited(self) -> bool:
        ...

    def get_number_of_files(self, directory_path: str) -> int:
        ...

    def verify_file_contents(self, directory_path: str, expected_contents: list[str]) -> bool:
        ...

    def log_app_output(self) -> bool:
        ...
