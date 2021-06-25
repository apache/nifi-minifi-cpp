import json
import logging
import subprocess
import sys
import time
import os

from .SingleNodeDockerCluster import SingleNodeDockerCluster
from .utils import retry_check


class DockerTestCluster(SingleNodeDockerCluster):
    def __init__(self):
        self.segfault = False

        super(DockerTestCluster, self).__init__()

    def deploy_flow(self):

        super(DockerTestCluster, self).deploy_flow()

    def start_flow(self, name):
        container = self.containers[name]
        container.reload()
        logging.info("Status before start: %s", container.status)
        if container.status == 'exited':
            logging.info("Start container: %s", name)
            container.start()
            return True
        return False

    def stop_flow(self, name):
        container = self.containers[name]
        container.reload()
        logging.info("Status before stop: %s", container.status)
        if container.status == 'running':
            logging.info("Stop container: %s", name)
            container.stop(timeout=0)
            return True
        return False

    @staticmethod
    def get_stdout_encoding():
        # Use UTF-8 both when sys.stdout present but set to None (explicitly piped output
        # and also some CI such as GitHub Actions).
        encoding = getattr(sys.stdout, "encoding", None)
        if encoding is None:
            encoding = "utf8"
        return encoding

    def get_app_log(self, container_id):
        container = self.client.containers.get(container_id)
        if b'Segmentation fault' in container.logs():
            logging.warn('Container segfaulted: %s', container.name)
            self.segfault = True

        try:
            apps = [("MiNiFi", self.minifi_root + '/logs/minifi-app.log'), ("NiFi", self.nifi_root + '/logs/nifi-app.log')]
            if container.status == 'running':
                for app in apps:
                    app_log_status, app_log = container.exec_run('/bin/sh -c \'cat ' + app[1] + '\'')
                    if app_log_status == 0:
                        return container.status, app_log
            elif container.status == 'exited':
                for app in apps:
                    log_file_name = container_id + ".log"
                    code = subprocess.run(["docker", "cp", container_id + ":" + app[1], log_file_name]).returncode
                    if code == 0:
                        output = open(log_file_name, 'rb').read()
                        os.remove(log_file_name)
                        return container.status, output
        except Exception:
            return container.status, None

        return container.status, container.logs()

    def wait_for_app_logs(self, log, timeout_seconds, count=1):
        wait_start_time = time.perf_counter()
        while (time.perf_counter() - wait_start_time) < timeout_seconds:
            for container_name, container in self.containers.items():
                logging.info('Waiting for app-logs `%s` in container `%s`', log, container_name)
                status, logs = self.get_app_log(container.id)
                if logs is not None and count <= logs.decode("utf-8").count(log):
                    return True
                elif status == 'exited':
                    return False
            time.sleep(1)
        return False

    def log_app_output(self):
        for container_name, container in self.containers.items():
            _, logs = self.get_app_log(container.id)
            if logs is not None:
                logging.info("Logs of container '%s':", container_name)
                for line in logs.decode("utf-8").splitlines():
                    logging.info(line)

    def check_minifi_container_started(self):
        for container in self.containers.values():
            container = self.client.containers.get(container.id)
            if b'Segmentation fault' in container.logs():
                logging.warn('Container segfaulted: %s', container.name)
                raise Exception("Container failed to start up.")

    def check_http_proxy_access(self, url):
        output = subprocess.check_output(["docker", "exec", "http-proxy", "cat", "/var/log/squid/access.log"]).decode(self.get_stdout_encoding())
        return url in output \
            and ((output.count("TCP_DENIED/407") != 0
                  and output.count("TCP_MISS") == output.count("TCP_DENIED/407"))
                 or output.count("TCP_DENIED/407") == 0 and "TCP_MISS" in output)

    @retry_check()
    def check_s3_server_object_data(self, test_data):
        s3_mock_dir = subprocess.check_output(["docker", "exec", "s3-server", "find", "/tmp/", "-type", "d", "-name", "s3mock*"]).decode(self.get_stdout_encoding()).strip()
        file_data = subprocess.check_output(["docker", "exec", "s3-server", "cat", s3_mock_dir + "/test_bucket/test_object_key/fileData"]).decode(self.get_stdout_encoding())
        return file_data == test_data

    @retry_check()
    def check_s3_server_object_metadata(self, content_type="application/octet-stream", metadata=dict()):
        s3_mock_dir = subprocess.check_output(["docker", "exec", "s3-server", "find", "/tmp/", "-type", "d", "-name", "s3mock*"]).decode(self.get_stdout_encoding()).strip()
        metadata_json = subprocess.check_output(["docker", "exec", "s3-server", "cat", s3_mock_dir + "/test_bucket/test_object_key/metadata"]).decode(self.get_stdout_encoding())
        server_metadata = json.loads(metadata_json)
        return server_metadata["contentType"] == content_type and metadata == server_metadata["userMetadata"]

    @retry_check()
    def check_azure_storage_server_data(self, test_data):
        data_file = subprocess.check_output(["docker", "exec", "azure-storage-server", "find", "/data/__blobstorage__", "-type", "f"]).decode(self.get_stdout_encoding()).strip()
        file_data = subprocess.check_output(["docker", "exec", "azure-storage-server", "cat", data_file]).decode(self.get_stdout_encoding())
        return test_data in file_data

    @retry_check()
    def is_s3_bucket_empty(self):
        s3_mock_dir = subprocess.check_output(["docker", "exec", "s3-server", "find", "/tmp/", "-type", "d", "-name", "s3mock*"]).decode(self.get_stdout_encoding()).strip()
        ls_result = subprocess.check_output(["docker", "exec", "s3-server", "ls", s3_mock_dir + "/test_bucket/"]).decode(self.get_stdout_encoding())
        return not ls_result

    def segfault_happened(self):
        return self.segfault

    def wait_for_kafka_consumer_to_be_registered(self):
        return self.wait_for_app_logs("Assignment received from leader for group docker_test_group", 60)
