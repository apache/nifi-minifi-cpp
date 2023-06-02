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
import os
import shutil
import hashlib


class DockerTestDirectoryBindings:
    def __init__(self, feature_id: str):
        self.data_directories = {}
        self.feature_id = feature_id

    def __del__(self):
        self.delete_data_directories()

    def cleanup_io(self):
        for folder in [self.data_directories[self.feature_id]["input_dir"], self.data_directories[self.feature_id]["output_dir"]]:
            for filename in os.listdir(folder):
                file_path = os.path.join(folder, filename)
                if os.path.isfile(file_path) or os.path.islink(file_path):
                    os.unlink(file_path)
                elif os.path.isdir(file_path):
                    shutil.rmtree(file_path)

    def create_new_data_directories(self):
        self.data_directories[self.feature_id] = {
            "input_dir": "/tmp/.nifi-test-input." + self.feature_id,
            "output_dir": "/tmp/.nifi-test-output." + self.feature_id,
            "resources_dir": "/tmp/.nifi-test-resources." + self.feature_id,
            "minifi_config_dir": "/tmp/.nifi-test-minifi-config-dir." + self.feature_id,
            "nifi_config_dir": "/tmp/.nifi-test-nifi-config-dir." + self.feature_id,
            "kubernetes_temp_dir": "/tmp/.nifi-test-kubernetes-temp-dir." + self.feature_id,
            "kubernetes_config_dir": "/tmp/.nifi-test-kubernetes-config-dir." + self.feature_id
        }

        [self.create_directory(directory) for directory in self.data_directories[self.feature_id].values()]

        # Add resources
        test_dir = os.environ['TEST_DIRECTORY']  # Based on DockerVerify.sh
        shutil.copytree(test_dir + "/resources/python", self.data_directories[self.feature_id]["resources_dir"] + "/python")
        shutil.copytree(test_dir + "/resources/opcua", self.data_directories[self.feature_id]["resources_dir"] + "/opcua")
        shutil.copytree(test_dir + "/resources/lua", self.data_directories[self.feature_id]["resources_dir"] + "/lua")
        shutil.copytree(test_dir + "/resources/minifi", self.data_directories[self.feature_id]["minifi_config_dir"], dirs_exist_ok=True)
        shutil.copytree(test_dir + "/resources/minifi-controller", self.data_directories[self.feature_id]["resources_dir"] + "/minifi-controller")

    def get_data_directories(self, feature_id):
        return self.data_directories[feature_id]

    def docker_path_to_local_path(self, feature_id, docker_path):
        # Docker paths are currently hard-coded
        if docker_path == "/tmp/input":
            return self.data_directories[feature_id]["input_dir"]
        if docker_path == "/tmp/output":
            return self.data_directories[feature_id]["output_dir"]
        if docker_path == "/tmp/resources":
            return self.data_directories[feature_id]["resources_dir"]
        # Might be worth reworking these
        if docker_path == "/tmp/output/success":
            self.create_directory(self.data_directories[feature_id]["output_dir"] + "/success")
            return self.data_directories[feature_id]["output_dir"] + "/success"
        if docker_path == "/tmp/output/failure":
            self.create_directory(self.data_directories[feature_id]["output_dir"] + "/failure")
            return self.data_directories[feature_id]["output_dir"] + "/failure"
        raise Exception("Docker directory \"%s\" has no preset bindings." % docker_path)

    def get_directory_bindings(self, feature_id):
        """
        Performs a standard container flow deployment with the addition
        of volumes supporting test input/output directories.
        """
        vols = {}
        vols[self.data_directories[feature_id]["input_dir"]] = {"bind": "/tmp/input", "mode": "rw"}
        vols[self.data_directories[feature_id]["output_dir"]] = {"bind": "/tmp/output", "mode": "rw"}
        vols[self.data_directories[feature_id]["resources_dir"]] = {"bind": "/tmp/resources", "mode": "rw"}
        vols[self.data_directories[feature_id]["minifi_config_dir"]] = {"bind": "/tmp/minifi_config", "mode": "rw"}
        vols[self.data_directories[feature_id]["nifi_config_dir"]] = {"bind": "/tmp/nifi_config", "mode": "rw"}
        vols[self.data_directories[feature_id]["kubernetes_config_dir"]] = {"bind": "/tmp/kubernetes_config", "mode": "rw"}
        return vols

    @staticmethod
    def create_directory(dir):
        os.makedirs(dir)
        os.chmod(dir, 0o777)

    @staticmethod
    def delete_tmp_directory(dir):
        assert dir.startswith("/tmp/")
        if not dir.endswith("/"):
            dir = dir + "/"
        # Sometimes rmtree does clean not up as expected, setting ignore_errors does not help either
        shutil.rmtree(dir, ignore_errors=True)

    def delete_data_directories(self):
        for directories in self.data_directories.values():
            for directory in directories.values():
                self.delete_tmp_directory(directory)

    @staticmethod
    def put_file_contents(file_abs_path, contents):
        logging.info('Writing %d bytes of content to file: %s', len(contents), file_abs_path)
        os.makedirs(os.path.dirname(file_abs_path), exist_ok=True)
        with open(file_abs_path, 'wb') as test_input_file:
            test_input_file.write(contents)
        os.chmod(file_abs_path, 0o0777)

    def put_test_resource(self, feature_id, file_name, contents):
        """
        Creates a resource file in the test resource dir and writes
        the given content to it.
        """

        file_abs_path = os.path.join(self.data_directories[feature_id]["resources_dir"], file_name)
        self.put_file_contents(file_abs_path, contents)

    def put_test_input(self, feature_id, file_name, contents):
        file_abs_path = os.path.join(self.data_directories[feature_id]["input_dir"], file_name)
        self.put_file_contents(file_abs_path, contents)

    def put_file_to_docker_path(self, feature_id, path, file_name, contents):
        file_abs_path = os.path.join(self.docker_path_to_local_path(feature_id, path), file_name)
        self.put_file_contents(file_abs_path, contents)

    @staticmethod
    def generate_md5_hash(file_path):
        with open(file_path, 'rb') as file:
            md5_hash = hashlib.md5()
            for chunk in iter(lambda: file.read(4096), b''):
                md5_hash.update(chunk)

        return md5_hash.hexdigest()

    def put_random_file_to_docker_path(self, test_id: str, path: str, file_name: str, file_size: int):
        file_abs_path = os.path.join(self.docker_path_to_local_path(test_id, path), file_name)
        with open(file_abs_path, 'wb') as test_input_file:
            test_input_file.write(os.urandom(file_size))
        os.chmod(file_abs_path, 0o0777)
        return self.generate_md5_hash(file_abs_path)

    def rm_out_child(self, feature_id, dir):
        child = os.path.join(self.data_directories[feature_id]["output_dir"], dir)
        logging.info('Removing %s from output folder', child)
        shutil.rmtree(child)
