# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from typing import Protocol

from minifi_behave.containers.directory import Directory
from minifi_behave.containers.file import File
from minifi_behave.containers.host_file import HostFile


class ContainerProtocol(Protocol):
    image_name: str
    container_name: str
    dirs: list[Directory]
    files: list[File]
    host_files: list[HostFile]

    def deploy(self, context) -> bool: ...

    def clean_up(self): ...

    def exec_run(self, command): ...

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool: ...

    def directory_contains_file_with_regex(self, directory_path: str, regex_str: str) -> bool: ...

    def path_with_content_exists(self, path: str, content: str) -> bool: ...

    def get_logs(self) -> str: ...

    @property
    def exited(self) -> bool: ...

    @property
    def exit_code(self) -> int: ...

    def get_number_of_files(self, directory_path: str) -> int: ...

    def verify_file_contents(self, directory_path: str, expected_contents: list[str]) -> bool: ...

    def log_app_output(self) -> bool: ...
