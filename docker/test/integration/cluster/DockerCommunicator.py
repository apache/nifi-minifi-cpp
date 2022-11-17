import docker
import uuid
import logging
import sys
import tempfile
import tarfile
import os
import io


class DockerCommunicator:
    def __init__(self):
        self.client = docker.from_env()

    def create_docker_network(self):
        net_name = 'minifi_integration_test_network-' + str(uuid.uuid4())
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
