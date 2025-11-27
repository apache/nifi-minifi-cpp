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
import docker
import logging
import sys
import tempfile
import tarfile
import os
import io
import uuid


class DockerCommunicator:
    def __init__(self):
        self.client = docker.from_env()

    def create_docker_network(self, feature_id: str):
        net_name = 'minifi_integration_test_network-' + feature_id
        logging.debug('Creating network: %s', net_name)
        return self.client.networks.create(net_name)

    @staticmethod
    def get_stdout_encoding():
        # Use UTF-8 both when sys.stdout present but set to None (explicitly piped output
        # and also some CI such as GitHub Actions).
        encoding = getattr(sys.stdout, "encoding", None)
        if encoding is None:
            encoding = "utf8"
        return encoding

    def execute_command(self, container_name, command):
        (code, output) = self.client.containers.get(container_name).exec_run(command)
        return (code, output.decode(self.get_stdout_encoding()))

    def get_app_log_from_docker_container(self, container_name):
        try:
            container = self.client.containers.get(container_name)
        except Exception:
            return 'not started', None

        if b'Segmentation fault' in container.logs():
            logging.warning('Container segfaulted: %s', container.name)
            self.segfault = True

        container.reload()
        return container.status, container.logs()

    def __put_archive(self, container_name, path, data):
        return self.client.containers.get(container_name).put_archive(path, data)

    def write_content_to_container(self, content, container_name, dst_path):
        with tempfile.TemporaryDirectory() as td:
            with tarfile.open(os.path.join(td, 'content.tar'), mode='w') as tar:
                info = tarfile.TarInfo(name=os.path.basename(dst_path))
                info.size = len(content)
                tar.addfile(info, io.BytesIO(content.encode('utf-8')))
            with open(os.path.join(td, 'content.tar'), 'rb') as data:
                return self.__put_archive(container_name, os.path.dirname(dst_path), data.read())

    def copy_file_from_container(self, container_name, src_path_in_container, dest_dir_on_host) -> bool:
        try:
            container = self.client.containers.get(container_name)
            (bits, _) = container.get_archive(src_path_in_container)
            tmp_tar_path = os.path.join(dest_dir_on_host, "retrieved_file_" + str(uuid.uuid4()) + ".tar")
            with open(tmp_tar_path, 'wb') as out_file:
                for chunk in bits:
                    out_file.write(chunk)
            with tarfile.open(tmp_tar_path, 'r') as tar:
                tar.extractall(dest_dir_on_host)
            os.remove(tmp_tar_path)
            return True
        except Exception as ex:
            logging.error('Exception occurred while copying file from container: %s', str(ex))
            return False
