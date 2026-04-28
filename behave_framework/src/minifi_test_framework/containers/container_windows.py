#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
from __future__ import annotations
import logging
import os
import tempfile
import base64
import tarfile
import io
from typing import TYPE_CHECKING

import docker
from docker.models.networks import Network
from docker.models.containers import Container

from minifi_test_framework.containers.container_protocol import ContainerProtocol
from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.host_file import HostFile

if TYPE_CHECKING:
    from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class WindowsContainer(ContainerProtocol):
    def __init__(self, image_name: str, container_name: str, network: Network, command: str | None = None, entrypoint: str | None = None):
        super().__init__()
        self.image_name: str = image_name
        self.container_name: str = container_name
        self.network: Network = network

        self.client: docker.DockerClient = docker.from_env()
        self.container: Container | None = None
        self.files: list[File] = []
        self.dirs: list[Directory] = []
        self.host_files: list[HostFile] = []
        self.volumes: dict = {}
        self.command: str | None = command
        self.entrypoint: str | None = entrypoint
        self._temp_dir: tempfile.TemporaryDirectory | None = None
        self.ports: dict[str, int] | None = None
        self.environment: list[str] = []

    def _normalize_path(self, path: str) -> str:
        clean_path = path.strip().replace("/", "\\")
        if clean_path.startswith("\\"):
            clean_path = clean_path[1:]

        # If it doesn't already have a drive letter, assume C:
        if ":" not in clean_path:
            return f"C:\\{clean_path}"
        return clean_path

    def deploy(self, context: MinifiTestContext | None) -> bool:
        if self._temp_dir:
            self._temp_dir.cleanup()
        self._temp_dir = tempfile.TemporaryDirectory()

        for directory in self.dirs:
            rel_path = directory.path.strip("/\\")
            temp_subdir = os.path.join(self._temp_dir.name, rel_path)
            os.makedirs(temp_subdir, exist_ok=True)

            for file_name, content in directory.files.items():
                file_path = os.path.join(temp_subdir, file_name)
                with open(file_path, "w", encoding="utf-8") as temp_file:
                    logging.info(f"writing content into {temp_file.name}")
                    temp_file.write(content)

            container_bind_path = self._normalize_path(directory.path)
            self.volumes[temp_subdir] = {
                "bind": container_bind_path,
                "mode": directory.mode
            }

        for host_file in self.host_files:
            container_bind_path = self._normalize_path(host_file.container_path)
            self.volumes[host_file.host_path] = {"bind": container_bind_path, "mode": host_file.mode}

        try:
            existing_container = self.client.containers.get(self.container_name)
            logging.warning(f"Found existing container '{self.container_name}'. Removing it first.")
            existing_container.remove(force=True)
        except docker.errors.NotFound:
            pass

        try:
            logging.info(f"Creating and starting container '{self.container_name}'...")
            self.container = self.client.containers.run(
                image=self.image_name,
                name=self.container_name,
                ports=self.ports,
                environment=self.environment,
                volumes=self.volumes,
                network=self.network.name,
                command=self.command,
                entrypoint=self.entrypoint,
                detach=True,
                tty=False
            )

            for file in self.files:
                self._copy_content_to_container(file.content, file.path)

        except Exception as e:
            logging.error(f"Error starting container: {e}")
            self.clean_up()
            raise
        return True

    def _copy_content_to_container(self, content: str | bytes, target_path: str):
        if not self.container:
            return

        win_path = self._normalize_path(target_path)
        dir_name = os.path.dirname(win_path)
        file_name = os.path.basename(win_path)

        self._run_powershell(f"New-Item -ItemType Directory -Force -Path '{dir_name}'")

        tar_stream = io.BytesIO()
        with tarfile.open(fileobj=tar_stream, mode='w') as tar:
            if isinstance(content, str):
                encoded_data = content.encode('utf-8')
            else:
                encoded_data = content
            tarinfo = tarfile.TarInfo(name=file_name)
            tarinfo.size = len(encoded_data)
            tar.addfile(tarinfo, io.BytesIO(encoded_data))

        tar_stream.seek(0)

        self.container.put_archive(path=dir_name, data=tar_stream)

    def clean_up(self):
        if self._temp_dir:
            try:
                self._temp_dir.cleanup()
            except Exception as e:
                logging.warning(f"Failed to cleanup temp dir: {e}")
            finally:
                self._temp_dir = None

        if self.container:
            try:
                self.container.remove(force=True)
            except docker.errors.NotFound:
                pass
            except Exception as e:
                logging.warning(f"Failed to remove container: {e}")
            finally:
                self.container = None

    def exec_run(self, command: str | list) -> tuple[int | None, str]:
        logging.debug(f"Running command: {command}")
        if self.container:
            (code, output) = self.container.exec_run(command, detach=False)
            decoded_output = output.decode("utf-8", errors='replace')
            logging.debug(f"Result {code}, output: {decoded_output}")
            return code, decoded_output
        return None, "Container not running."

    def _run_powershell(self, ps_script: str) -> tuple[int | None, str]:
        if not self.container:
            return None, "Container not running"

        encoded_command = base64.b64encode(ps_script.encode('utf_16_le')).decode('utf-8')

        cmd_parts = ["powershell", "-NonInteractive", "-NoProfile", "-EncodedCommand", encoded_command]

        return self.exec_run(cmd_parts)

    def not_empty_dir_exists(self, directory_path: str) -> bool:
        if not self.container:
            return False

        win_path = self._normalize_path(directory_path)
        ps_script = (
            f"if (Test-Path -Path '{win_path}' -PathType Container) {{ "
            f"  if (Get-ChildItem -Path '{win_path}') {{ exit 0 }} else {{ exit 1 }} "
            f"}} else {{ exit 2 }}"
        )

        exit_code, _ = self._run_powershell(ps_script)
        return exit_code == 0

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container:
            return False

        win_path = self._normalize_path(directory_path)
        escaped_content = expected_content.replace("'", "''")

        ps_script = (
            f"if (-not (Test-Path '{win_path}')) {{ exit 2 }}; "
            f"$matches = Get-ChildItem -Path '{win_path}' -File -Depth 0 | "
            f"Select-String -Pattern '{escaped_content}' -SimpleMatch -List; "
            f"if ($matches) {{ exit 0 }} else {{ exit 1 }}"
        )

        exit_code, _ = self._run_powershell(ps_script)
        return exit_code == 0

    def directory_contains_file_with_regex(self, directory_path: str, regex_str: str) -> bool:
        if not self.container:
            return False

        win_path = self._normalize_path(directory_path)
        escaped_regex = regex_str.replace("'", "''")

        ps_script = (
            f"if (-not (Test-Path '{win_path}')) {{ exit 2 }}; "
            f"$matches = Get-ChildItem -Path '{win_path}' -File -Depth 0 | "
            f"Select-String -Pattern '{escaped_regex}' -List; "
            f"if ($matches) {{ exit 0 }} else {{ exit 1 }}"
        )

        exit_code, _ = self._run_powershell(ps_script)
        return exit_code == 0

    def path_with_content_exists(self, path: str, content: str) -> bool:
        if not self.container:
            return False

        win_path = self._normalize_path(path)

        escaped_content = content.replace("'", "''").replace("\n", "\r\n")

        ps_script = (
            f"try {{ "
            f"  $found = Select-String -Path '{win_path}' -Pattern '^{escaped_content}$'; "
            f"  if ($found.Count -eq 1) {{ exit 0 }} else {{ exit 1 }} "
            f"}} catch {{ exit 2 }}"
        )

        exit_code, output = self._run_powershell(ps_script)
        if exit_code != 0:
            logging.debug(f"path_with_content_exists failed for {win_path}. Output: {output}")

        return exit_code == 0

    def directory_has_single_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container:
            return False

        win_path = self._normalize_path(directory_path)
        escaped_content = expected_content.strip().replace("'", "''").replace("\n", "\r\n")

        ps_script = (
            f"$files = Get-ChildItem -Path '{win_path}' -File -Depth 0; "
            f"if ($files.Count -ne 1) {{ exit 1 }}; "
            f"$actual_content = Get-Content -Path $files[0].FullName -Raw; "
            f"if ($actual_content.Trim() -eq '{escaped_content}') {{ exit 0 }} else {{ exit 2 }};"
        )

        exit_code, output = self._run_powershell(ps_script)
        if exit_code != 0:
            logging.debug(f"Check for single file failed in {win_path}. Output: {output}")

        return exit_code == 0

    def get_logs(self) -> str:
        logging.debug("Getting logs from container '%s'", self.container_name)
        if not self.container:
            return ""
        logs_as_bytes = self.container.logs()
        return logs_as_bytes.decode('utf-8', errors='replace')

    def log_app_output(self) -> bool:
        logs = self.get_logs()
        logging.info("Logs of container '%s':", self.container_name)
        for line in logs.splitlines():
            logging.info(line)
        return False

    @property
    def exited(self) -> bool:
        if not self.container:
            return False
        try:
            self.container.reload()
            return self.container.status == 'exited'
        except docker.errors.NotFound:
            self.container = None
            return False
        except Exception:
            return False

    def get_number_of_files(self, directory_path: str) -> int:
        if not self.container:
            return -1

        win_path = self._normalize_path(directory_path)
        ps_script = f"(Get-ChildItem -Path '{win_path}' -File -Depth 0).Count"

        exit_code, output = self._run_powershell(ps_script)

        if exit_code != 0:
            logging.error(f"Error counting files in '{win_path}': {output}")
            return -1

        try:
            return int(output.strip())
        except (ValueError, IndexError):
            return -1

    def verify_file_contents(self, directory_path: str, expected_contents: list[str]) -> bool:
        if not self.container:
            return False

        win_path = self._normalize_path(directory_path)

        ps_list = f"Get-ChildItem -Path '{win_path}' -File -Depth 0 | Select-Object -ExpandProperty FullName"
        exit_code, output = self._run_powershell(ps_list)

        if exit_code != 0:
            logging.error(f"Error listing files in '{win_path}': {output}")
            return False

        actual_filepaths = [path.strip() for path in output.splitlines() if path.strip()]

        if len(actual_filepaths) != len(expected_contents):
            return False

        actual_file_contents = []
        for path in actual_filepaths:
            ps_read = (
                f"$c = Get-Content -Path '{path}' -Raw; "
                f"if ($c) {{ ($c -replace '\\r\\n', '\\n' -replace '\\r', '\\n').Trim() }}"
            )

            exit_code, content = self._run_powershell(ps_read)
            if exit_code != 0:
                return False
            actual_file_contents.append(content)

        normalized_expected = [
            s.strip().replace("\r\n", "\n").replace("\r", "\n") for s in expected_contents
        ]

        return sorted(actual_file_contents) == sorted(normalized_expected)

    def nonempty_dir_exists(self, directory_path: str) -> bool:
        if not self.container:
            return False

        win_path = self._normalize_path(directory_path)
        ps_script = (
            f"if ((Test-Path -LiteralPath '{win_path}' -PathType Container) "
            f"-and ((Get-ChildItem -LiteralPath '{win_path}' -Force | "
            f"        Select-Object -First 1 | Measure-Object).Count -gt 0)) "
            f"{{ exit 0 }} else {{ exit 1 }}"
        )

        exit_code, output = self._run_powershell(ps_script)

        if exit_code != 0:
            logging.error(f"Error running command for nonempty_dir_exists: {output}")
            return False

        return True

    def directory_contains_file_with_minimum_size(self, directory_path: str, expected_size: int) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            return False

        win_path = self._normalize_path(directory_path)
        ps_script = (
            f"Get-ChildItem -LiteralPath '{win_path}' -File "
            f"| Where-Object {{ $_.Length -gt {expected_size} }}"
        )

        exit_code, output = self._run_powershell(ps_script)

        if exit_code != 0:
            logging.error(f"Error running command to get file sizes: {output}")
            return False

        if output and len(output.strip()) > 0:
            return True

        return False
