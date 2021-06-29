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
        super(DockerTestCluster, self).__init__()
        self.segfault = False

    @staticmethod
    def get_stdout_encoding():
        # Use UTF-8 both when sys.stdout present but set to None (explicitly piped output
        # and also some CI such as GitHub Actions).
        encoding = getattr(sys.stdout, "encoding", None)
        if encoding is None:
            encoding = "utf8"
        return encoding

    def get_app_log(self, container_name):
        try:
            container = self.client.containers.get(container_name)
        except Exception:
            return 'not started', None

        if b'Segmentation fault' in container.logs():
            logging.warn('Container segfaulted: %s', container.name)
            self.segfault = True

        log_file_path = self.containers[container_name].get_log_file_path()
        if not log_file_path:
            return container.status, container.logs()

        try:
            if container.status == 'running':
                app_log_status, app_log = container.exec_run('/bin/sh -c \'cat ' + log_file_path + '\'')
                if app_log_status == 0:
                    return container.status, app_log
            elif container.status == 'exited':
                log_file_name = container_name + ".log"
                code = subprocess.run(["docker", "cp", container_name + ":" + log_file_path, log_file_name]).returncode
                if code == 0:
                    output = open(log_file_name, 'rb').read()
                    os.remove(log_file_name)
                    return container.status, output
        except Exception:
            return container.status, None

        return container.status, None

    def wait_for_app_logs(self, container_name, log_entry, timeout_seconds, count=1):
        wait_start_time = time.perf_counter()
        while (time.perf_counter() - wait_start_time) < timeout_seconds:
            logging.info('Waiting for app-logs `%s` in container `%s`', log_entry, container_name)
            status, logs = self.get_app_log(container_name)
            if logs is not None and count <= logs.decode("utf-8").count(log_entry):
                return True
            elif status == 'exited':
                return False
            time.sleep(1)
        return False

    def wait_for_startup_log(self, container_name, timeout_seconds):
        return self.wait_for_app_logs(container_name, self.containers[container_name].get_startup_finished_log_entry(), timeout_seconds, 1)

    def log_app_output(self):
        for container_name in self.containers:
            _, logs = self.get_app_log(container_name)
            if logs is not None:
                logging.info("Logs of container '%s':", container_name)
                for line in logs.decode("utf-8").splitlines():
                    logging.info(line)

    def check_http_proxy_access(self, url):
        output = subprocess.check_output(["docker", "exec", "http-proxy", "cat", "/var/log/squid/access.log"]).decode(self.get_stdout_encoding())
        return url in output \
            and ((output.count("TCP_DENIED") != 0
                  and output.count("TCP_MISS") == output.count("TCP_DENIED"))
                 or output.count("TCP_DENIED") == 0 and "TCP_MISS" in output)

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

    def query_postgres_server(self, query, number_of_rows):
        return str(number_of_rows) + " rows" in subprocess.check_output(["docker", "exec", "postgresql-server", "psql", "-U", "postgres", "-c", query]).decode(self.get_stdout_encoding()).strip()

    def check_query_results(self, query, number_of_rows, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.query_postgres_server(query, number_of_rows):
                return True
            time.sleep(2)
        return False

    def segfault_happened(self):
        return self.segfault

    def wait_for_kafka_consumer_to_be_registered(self, kafka_container_name):
        return self.wait_for_app_logs(kafka_container_name, "Assignment received from leader for group docker_test_group", 60)
