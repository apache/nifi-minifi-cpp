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
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
import json
import logging
import os
import shlex
import tempfile
import tarfile
import uuid

import docker
from docker.models.networks import Network

from minifi_test_framework.containers.directory import Directory
from minifi_test_framework.containers.file import File
from minifi_test_framework.containers.host_file import HostFile


class Container:
    def __init__(self, image_name: str, container_name: str, network: Network, command: str | None = None, entrypoint: str | None = None):
        self.image_name: str = image_name
        self.container_name: str = container_name
        self.network: Network = network
        self.user: str = "0:0"
        self.client: docker.DockerClient = docker.from_env()
        self.container = None
        self.files: list[File] = []
        self.dirs: list[Directory] = []
        self.host_files: list[HostFile] = []
        self.volumes = {}
        self.command: str | None = command
        self.entrypoint: str | None = entrypoint
        self._temp_dir: tempfile.TemporaryDirectory | None = None
        self.ports: dict[str, int] | None = None
        self.environment: list[str] = []

    def add_host_file(self, host_path: str, container_path: str, mode: str = "ro"):
        self.host_files.append(HostFile(container_path, host_path, mode))

    def add_file_to_running_container(self, content: str, path: str):
        if not self.container:
            logging.error("Container is not running. Cannot add file.")
            raise RuntimeError("Container is not running. Cannot add file.")

        mkdir_command = f"mkdir -p {shlex.quote(path)}"
        exit_code, output = self.exec_run(mkdir_command)
        if exit_code != 0:
            logging.error(f"Error creating directory '{path}' in container: {output}")
            raise RuntimeError(f"Error creating directory '{path}' in container: {output}")

        file_name = str(uuid.uuid4())
        exit_code, output = self.exec_run(f"sh -c \"printf %s '{content}' > {os.path.join(path, file_name)}\"")
        if exit_code != 0:
            logging.error(f"Error adding file to running container: {output}")
            raise RuntimeError(f"Error adding file to running container: {output}")

    def _write_content_to_file(self, filepath: str, permissions: int | None, content: str | bytes):
        write_mode = "w"
        if isinstance(content, bytes):
            write_mode = "wb"
        with open(filepath, write_mode) as file:
            file.write(content)
        if permissions:
            os.chmod(filepath, permissions)

    def _configure_volumes_of_container_files(self):
        for file in self.files:
            temp_path = os.path.join(self._temp_dir.name, os.path.basename(file.path))
            self._write_content_to_file(temp_path, file.permissions, file.content)
            self.volumes[temp_path] = {"bind": file.path, "mode": file.mode}

    def _configure_volumes_of_container_dirs(self):
        for directory in self.dirs:
            temp_path = self._temp_dir.name + directory.path
            os.makedirs(temp_path, exist_ok=True)
            for file_name, content in directory.files.items():
                file_path = os.path.join(temp_path, file_name)
                self._write_content_to_file(file_path, None, content)
            self.volumes[temp_path] = {"bind": directory.path, "mode": directory.mode}

    def is_deployed(self) -> bool:
        return self.container is not None

    def deploy(self) -> bool:
        if self.is_deployed():
            logging.info(f"Container '{self.container_name}' is already deployed.")
            return True
        self._temp_dir = tempfile.TemporaryDirectory()
        self._configure_volumes_of_container_files()
        self._configure_volumes_of_container_dirs()
        for host_file in self.host_files:
            self.volumes[host_file.host_path] = {"bind": host_file.container_path, "mode": host_file.mode}

        try:
            existing_container = self.client.containers.get(self.container_name)
            logging.warning(f"Found existing container '{self.container_name}'. Removing it first.")
            existing_container.remove(force=True)
        except docker.errors.NotFound:
            pass
        try:
            logging.info(f"Creating and starting container '{self.container_name}'...")
            self.container = self.client.containers.run(
                image=self.image_name, name=self.container_name, ports=self.ports,
                environment=self.environment, volumes=self.volumes, network=self.network.name,
                command=self.command, entrypoint=self.entrypoint, user=self.user, detach=True)
        except Exception as e:
            logging.error(f"Error starting container: {e}")
            raise
        return True

    def start(self):
        if self.container:
            self.container.start()
        else:
            logging.error("Container does not exist. Cannot start.")

    def stop(self):
        if self.container:
            self.container.stop()
        else:
            logging.error("Container does not exist. Cannot stop.")

    def kill(self):
        if self.container:
            self.container.kill()
        else:
            logging.error("Container does not exist. Cannot kill.")

    def restart(self):
        if self.container:
            self.container.restart()
        else:
            logging.error("Container does not exist. Cannot restart.")

    def clean_up(self):
        if self.container:
            try:
                self.container.remove(force=True)
            except Exception as e:
                logging.error(f"Error cleaning up container '{self.container_name}': {e}")

    def exec_run(self, command) -> tuple[int | None, str]:
        if self.container:
            (code, output) = self.container.exec_run(command)
            return code, output.decode("utf-8")
        return None, "Container not running."

    def nonempty_dir_exists(self, directory_path: str) -> bool:
        if not self.container:
            return False
        dir_exists_exit_code, dir_exists_output = self.exec_run(
            "sh -c {}".format(shlex.quote(f"test -d {directory_path}")))
        if dir_exists_exit_code != 0:
            return False
        dir_not_empty_ec, dir_not_empty_output = self.exec_run(
            "sh -c {}".format(shlex.quote(f'[ "$(ls -A {directory_path})" ]')))
        return dir_not_empty_ec == 0

    def directory_contains_empty_file(self, directory_path: str) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            return False

        command = "sh -c {}".format(shlex.quote(f"find {directory_path} -maxdepth 1 -type f -empty"))

        exit_code, _ = self.exec_run(command)

        return exit_code == 0

    def directory_contains_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            return False

        quoted_content = shlex.quote(expected_content)
        command = "sh -c {}".format(shlex.quote(f"grep -l -F -- {quoted_content} {directory_path}/*"))

        exit_code, _ = self.exec_run(command)

        return exit_code == 0

    def directory_contains_file_with_regex(self, directory_path: str, regex_str: str) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            return False

        safe_dir_path = shlex.quote(directory_path)
        safe_regex_str = shlex.quote(regex_str)

        command = (f"find {safe_dir_path} -maxdepth 1 -type f -print0 | "
                   f"xargs -0 -r grep -l -E -- {safe_regex_str}")

        exit_code, output = self.exec_run(f"sh -c \"{command}\"")

        if exit_code != 0:
            logging.warning(f"directory_contains_file_with_regex {output}")
        return exit_code == 0

    def path_with_content_exists(self, path: str, content: str) -> bool:
        count_command = f"sh -c 'cat {path} | grep \"^{content}$\" | wc -l'"
        exit_code, output = self.exec_run(count_command)
        if exit_code != 0:
            logging.error(f"Error running command '{count_command}': {output}")
            return False

        try:
            file_count = int(output.strip())
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{count_command}'")
            return False
        return file_count == 1

    def directory_has_single_file_with_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            return False

        count_command = f"sh -c 'find {directory_path} -maxdepth 1 -type f | wc -l'"
        exit_code, output = self.exec_run(count_command)

        if exit_code != 0:
            logging.error(f"Error running command '{count_command}': {output}")
            return False

        try:
            file_count = int(output.strip())
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{count_command}'")
            return False

        if file_count != 1:
            logging.error(f"{directory_path} has too many or too few ({file_count}) files")
            return False

        content_command = f"sh -c 'cat {directory_path}/*'"
        exit_code, output = self.exec_run(content_command)

        if exit_code != 0:
            logging.error(f"Error running command '{content_command}': {output}")
            return False

        actual_content = output.strip()
        logging.debug(f"Comparing: '{actual_content}' vs {expected_content}")
        return actual_content == expected_content.strip()

    def get_logs(self) -> str:
        logging.debug("Getting logs from container '%s'", self.container_name)
        if not self.container:
            return ""
        logs_as_bytes = self.container.logs()
        return logs_as_bytes.decode('utf-8')

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
            logging.warning("Container not running")
            return -1

        if not self.nonempty_dir_exists(directory_path):
            logging.warning(f"Container directory does not exist: {directory_path}")
            return 0

        count_command = f"sh -c 'find {directory_path} -maxdepth 1 -type f | wc -l'"
        exit_code, output = self.exec_run(count_command)

        if exit_code != 0:
            logging.error(f"Error running command '{count_command}': {output}")
            return -1

        try:
            file_count = int(output.strip())
            logging.debug(f"Number of files in '{directory_path}': {file_count}")
            return file_count
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{count_command}'")
            return -1

    def _get_contents_of_all_files_in_directory(self, directory_path: str) -> list[str] | None:
        safe_dir_path = shlex.quote(directory_path)
        list_files_command = f"find {safe_dir_path} -mindepth 1 -maxdepth 1 -type f -print0"

        exit_code, output = self.exec_run(f"sh -c \"{list_files_command}\"")

        if exit_code != 0:
            logging.error(f"Error running command '{list_files_command}': {output}")
            return None

        actual_filepaths = [path for path in output.split('\0') if path]

        actual_file_contents = []
        for path in actual_filepaths:
            safe_path = shlex.quote(path)
            read_command = f"cat {safe_path}"

            exit_code, content = self.exec_run(read_command)

            if exit_code != 0:
                error_message = f"Command to read file '{path}' failed with exit code {exit_code}"
                logging.error(error_message)
                return None

            actual_file_contents.append(content)

        return actual_file_contents

    def _verify_file_contents_in_running_container(self, directory_path: str, expected_contents: list[str]) -> bool:
        if not self.nonempty_dir_exists(directory_path):
            return False

        actual_file_contents = self._get_contents_of_all_files_in_directory(directory_path)
        if actual_file_contents is None:
            return False

        if len(actual_file_contents) != len(expected_contents):
            logging.debug(f"Expected {len(expected_contents)} files, but found {len(actual_file_contents)}")
            return False

        return sorted(actual_file_contents) == sorted(expected_contents)

    def _verify_file_contents_in_stopped_container(self, directory_path: str, expected_contents: list[str]) -> bool:
        if not self.container:
            return False

        with tempfile.TemporaryDirectory() as temp_dir:
            extracted_dir = self._extract_directory_from_container(directory_path, temp_dir)
            if not extracted_dir:
                return False

            actual_file_contents = self._read_files_from_directory(extracted_dir)
            if actual_file_contents is None:
                return False

            return sorted(actual_file_contents) == sorted(expected_contents)

    def _extract_directory_from_container(self, directory_path: str, temp_dir: str) -> str | None:
        try:
            bits, _ = self.container.get_archive(directory_path)
            temp_tar_path = os.path.join(temp_dir, "archive.tar")

            with open(temp_tar_path, 'wb') as f:
                for chunk in bits:
                    f.write(chunk)

            with tarfile.open(temp_tar_path) as tar:
                tar.extractall(path=temp_dir)

            return os.path.join(temp_dir, os.path.basename(directory_path.strip('/')))
        except Exception as e:
            logging.error(f"Error extracting files from directory path '{directory_path}' from container '{self.container_name}': {e}")
            return None

    def _read_files_from_directory(self, directory_path: str) -> list[str] | None:
        try:
            file_contents = []
            for entry in os.scandir(directory_path):
                if entry.is_file():
                    with open(entry.path, 'r') as f:
                        file_contents.append(f.read())
            return file_contents
        except Exception as e:
            logging.error(f"Error reading extracted files: {e}")
            return None

    def verify_file_contents(self, directory_path: str, expected_contents: list[str]) -> bool:
        if not self.container:
            return False

        self.container.reload()
        if self.container.status == "running":
            return self._verify_file_contents_in_running_container(directory_path, expected_contents)

        return self._verify_file_contents_in_stopped_container(directory_path, expected_contents)

    def log_app_output(self) -> bool:
        logs = self.get_logs()
        logging.info("Logs of container '%s':", self.container_name)
        for line in logs.splitlines():
            logging.info(line)
        return False

    def verify_path_with_json_content(self, directory_path: str, expected_str: str) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            logging.warning(f"Container not running or directory does not exist: {directory_path}")
            return False

        count_command = f"sh -c 'find {directory_path} -maxdepth 1 -type f | wc -l'"
        exit_code, output = self.exec_run(count_command)

        if exit_code != 0:
            logging.error(f"Error running command '{count_command}': {output}")
            return False

        try:
            file_count = int(output.strip())
        except (ValueError, IndexError):
            logging.error(f"Error parsing output '{output}' from command '{count_command}'")
            return False

        if file_count != 1:
            logging.error(f"{directory_path} has too many or too few ({file_count}) files")
            return False

        content_command = f"sh -c 'cat {directory_path}/*'"
        exit_code, output = self.exec_run(content_command)

        if exit_code != 0:
            logging.error(f"Error running command '{content_command}': {output}")
            return False

        actual_content = output.strip()
        actual_json = json.loads(actual_content)
        expected_json = json.loads(expected_str)

        return actual_json == expected_json

    def directory_contains_file_with_json_content(self, directory_path: str, expected_content: str) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            logging.warning(f"Container not running or directory does not exist: {directory_path}")
            return False

        actual_file_contents = self._get_contents_of_all_files_in_directory(directory_path)
        if actual_file_contents is None:
            return False

        for file_content in actual_file_contents:
            try:
                actual_json = json.loads(file_content)
                expected_json = json.loads(expected_content)
                if actual_json == expected_json:
                    return True
                logging.warning(f"File content does not match expected JSON: {file_content}")
            except json.JSONDecodeError:
                logging.error("Error decoding JSON content from file.")
                continue

        return False

    def directory_contains_file_with_minimum_size(self, directory_path: str, expected_size: int) -> bool:
        if not self.container or not self.nonempty_dir_exists(directory_path):
            return False

        command = f"find \"{directory_path}\" -maxdepth 1 -type f -size +{expected_size}c"

        exit_code, output = self.exec_run(command)
        if exit_code != 0:
            logging.error(f"Error running command to get file sizes: {output}")
            return False
        if len(output.strip()) > 0:
            return True

        return False
