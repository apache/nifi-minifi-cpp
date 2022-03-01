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


import glob
import os
import stat
import subprocess
import time
from textwrap import dedent


class KindProxy:
    def __init__(self, temp_directory):
        self.temp_directory = temp_directory
        self.kind_binary_path = os.path.join(self.temp_directory, 'kind')
        self.kind_config_path = os.path.join(self.temp_directory, 'kind-config.yml')
        self.__download_kind()

    def __download_kind(self):
        if subprocess.run(['curl', '-Lo', self.kind_binary_path, 'https://kind.sigs.k8s.io/dl/v0.11.1/kind-linux-amd64']).returncode != 0:
            raise Exception("Could not download kind")
        os.chmod(self.kind_binary_path, stat.S_IXUSR)

    def create_config(self, volumes):
        kind_config = dedent("""\
                apiVersion: kind.x-k8s.io/v1alpha4
                kind: Cluster
                nodes:
                   - role: control-plane
                """)

        if volumes:
            kind_config += "     extraMounts:\n"

        for host_path, container_path in volumes.items():
            kind_config += "      - hostPath: {path}\n".format(path=host_path)
            kind_config += "        containerPath: {path}\n".format(path=container_path['bind'])
            if container_path['mode'] != 'rw':
                kind_config += "        readOnly: true\n"

        with open(self.kind_config_path, 'wb') as config_file:
            config_file.write(kind_config.encode('utf-8'))

    def start_cluster(self):
        subprocess.run([self.kind_binary_path, 'delete', 'cluster'])

        if subprocess.run([self.kind_binary_path, 'create', 'cluster', '--config=' + self.kind_config_path]).returncode != 0:
            raise Exception("Could not start the kind cluster")

    def load_docker_image(self, image_store):
        image = image_store.get_image('minifi-cpp-in-kubernetes')
        image.tag(repository='minifi-kubernetes-test', tag='v1')

        if subprocess.run([self.kind_binary_path, 'load', 'docker-image', 'minifi-kubernetes-test:v1']).returncode != 0:
            raise Exception("Could not load the minifi docker image into the kind cluster")

    def create_objects(self):
        test_dir = os.environ['TEST_DIRECTORY']
        resources_directory = os.path.join(test_dir, 'resources', 'kubernetes', 'pods-etc')

        self.__wait_for_default_service_account('default')
        namespaces = self.__create_objects_of_type(resources_directory, 'namespace')
        for namespace in namespaces:
            self.__wait_for_default_service_account(namespace)

        self.__create_objects_of_type(resources_directory, 'pod')
        self.__create_objects_of_type(resources_directory, 'clusterrole')
        self.__create_objects_of_type(resources_directory, 'clusterrolebinding')

    def __wait_for_default_service_account(self, namespace):
        for _ in range(120):
            if subprocess.run(['docker', 'exec', 'kind-control-plane', 'kubectl', '-n', namespace, 'get', 'serviceaccount', 'default']).returncode == 0:
                return
            time.sleep(1)
        raise Exception("Default service account for namespace '%s' not found" % namespace)

    def __create_objects_of_type(self, directory, type):
        found_objects = []
        for file_name in glob.iglob(os.path.join(directory, f'*.{type}.yml')):
            file_handle = os.open(file_name, os.O_RDONLY)
            if subprocess.run(['docker', 'exec', '-i', 'kind-control-plane', 'kubectl', 'apply', '-f', '-'], stdin=file_handle).returncode != 0:
                raise Exception("Could not create kubernetes object from file '%s'" % file_name)
            object_name = os.path.basename(file_name).replace(f'.{type}.yml', '')
            found_objects.append(object_name)
        return found_objects

    def get_logs(self, namespace, pod_name):
        kubectl_logs_output = subprocess.run(['docker', 'exec', 'kind-control-plane', 'kubectl', '-n', namespace, 'logs', pod_name], capture_output=True)
        if kubectl_logs_output.returncode == 0:
            return kubectl_logs_output.stdout
        else:
            return None

    def cleanup(self):
        # cleanup gets called multiple times, also after the temp directories had been removed
        if os.path.exists(self.kind_binary_path):
            subprocess.run([self.kind_binary_path, 'delete', 'cluster'])
