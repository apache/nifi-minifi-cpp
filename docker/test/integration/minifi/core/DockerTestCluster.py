import json
import logging
import subprocess
import sys
import time
import os
import re

from .SingleNodeDockerCluster import SingleNodeDockerCluster
from .utils import retry_check


class DockerTestCluster(SingleNodeDockerCluster):
    def __init__(self, image_store):
        super(DockerTestCluster, self).__init__(image_store)
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
            logging.warning('Container segfaulted: %s', container.name)
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

    def __wait_for_app_logs_impl(self, container_name, log_entry, timeout_seconds, count, use_regex):
        wait_start_time = time.perf_counter()
        while (time.perf_counter() - wait_start_time) < timeout_seconds:
            logging.info('Waiting for app-logs `%s` in container `%s`', log_entry, container_name)
            status, logs = self.get_app_log(container_name)
            if logs is not None:
                if not use_regex and logs.decode("utf-8").count(log_entry) >= count:
                    return True
                elif use_regex and len(re.findall(log_entry, logs.decode("utf-8"))) >= count:
                    return True
            elif status == 'exited':
                return False
            time.sleep(1)
        return False

    def wait_for_app_logs_regex(self, container_name, log_entry, timeout_seconds, count=1):
        return self.__wait_for_app_logs_impl(container_name, log_entry, timeout_seconds, count, True)

    def wait_for_app_logs(self, container_name, log_entry, timeout_seconds, count=1):
        return self.__wait_for_app_logs_impl(container_name, log_entry, timeout_seconds, count, False)

    def wait_for_startup_log(self, container_name, timeout_seconds):
        return self.wait_for_app_logs(container_name, self.containers[container_name].get_startup_finished_log_entry(), timeout_seconds, 1)

    def log_app_output(self):
        for container_name in self.containers:
            _, logs = self.get_app_log(container_name)
            if logs is not None:
                logging.info("Logs of container '%s':", container_name)
                for line in logs.decode("utf-8").splitlines():
                    logging.info(line)

    def check_http_proxy_access(self, container_name, url):
        (code, output) = self.client.containers.get(container_name).exec_run(["cat", "/var/log/squid/access.log"])
        output = output.decode(self.get_stdout_encoding())
        return code == 0 and url in output \
            and ((output.count("TCP_DENIED") != 0
                 and output.count("TCP_MISS") == output.count("TCP_DENIED"))
                 or output.count("TCP_DENIED") == 0 and "TCP_MISS" in output)

    @retry_check()
    def check_s3_server_object_data(self, container_name, test_data):
        (code, output) = self.client.containers.get(container_name).exec_run(["find", "/tmp/", "-type", "d", "-name", "s3mock*"])
        if code != 0:
            return False
        s3_mock_dir = output.decode(self.get_stdout_encoding()).strip()
        (code, output) = self.client.containers.get(container_name).exec_run(["cat", s3_mock_dir + "/test_bucket/test_object_key/fileData"])
        file_data = output.decode(self.get_stdout_encoding())
        return code == 0 and file_data == test_data

    @retry_check()
    def check_s3_server_object_metadata(self, container_name, content_type="application/octet-stream", metadata=dict()):
        (code, output) = self.client.containers.get(container_name).exec_run(["find", "/tmp/", "-type", "d", "-name", "s3mock*"])
        if code != 0:
            return False
        s3_mock_dir = output.decode(self.get_stdout_encoding()).strip()
        (code, output) = self.client.containers.get(container_name).exec_run(["cat", s3_mock_dir + "/test_bucket/test_object_key/metadata"])
        server_metadata = json.loads(output.decode(self.get_stdout_encoding()))
        return code == 0 and server_metadata["contentType"] == content_type and metadata == server_metadata["userMetadata"]

    @retry_check()
    def check_azure_storage_server_data(self, container_name, test_data):
        (code, output) = self.client.containers.get(container_name).exec_run(["find", "/data/__blobstorage__", "-type", "f"])
        if code != 0:
            return False
        data_file = output.decode(self.get_stdout_encoding()).strip()
        (code, output) = self.client.containers.get(container_name).exec_run(["cat", data_file])
        file_data = output.decode(self.get_stdout_encoding())
        return code == 0 and test_data in file_data

    @retry_check()
    def is_s3_bucket_empty(self, container_name):
        (code, output) = self.client.containers.get(container_name).exec_run(["find", "/tmp/", "-type", "d", "-name", "s3mock*"])
        if code != 0:
            return False
        s3_mock_dir = output.decode(self.get_stdout_encoding()).strip()
        (code, output) = self.client.containers.get(container_name).exec_run(["ls", s3_mock_dir + "/test_bucket/"])
        ls_result = output.decode(self.get_stdout_encoding())
        return code == 0 and not ls_result

    def query_postgres_server(self, postgresql_container_name, query, number_of_rows):
        (code, output) = self.client.containers.get(postgresql_container_name).exec_run(["psql", "-U", "postgres", "-c", query])
        output = output.decode(self.get_stdout_encoding())
        return code == 0 and str(number_of_rows) + " rows" in output

    def check_query_results(self, postgresql_container_name, query, number_of_rows, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.query_postgres_server(postgresql_container_name, query, number_of_rows):
                return True
            time.sleep(2)
        return False

    def segfault_happened(self):
        return self.segfault

    def wait_for_kafka_consumer_to_be_registered(self, kafka_container_name):
        return self.wait_for_app_logs(kafka_container_name, "Assignment received from leader for group docker_test_group", 60)
