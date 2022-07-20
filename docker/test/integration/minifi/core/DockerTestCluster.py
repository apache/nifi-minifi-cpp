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
import json
import logging
import sys
import time
import os
import re
import tarfile
import io
import tempfile

from .LogSource import LogSource
from .SingleNodeDockerCluster import SingleNodeDockerCluster
from .PrometheusChecker import PrometheusChecker
from .utils import retry_check
from azure.storage.blob import BlobServiceClient
from azure.core.exceptions import ResourceExistsError


class DockerTestCluster(SingleNodeDockerCluster):
    AZURE_CONNECTION_STRING = \
        ("DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;"
         "BlobEndpoint=http://127.0.0.1:10000/devstoreaccount1;QueueEndpoint=http://127.0.0.1:10001/devstoreaccount1;")

    def __init__(self, context):
        super(DockerTestCluster, self).__init__(context)
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
        log_source = self.containers[container_name].log_source()
        if log_source == LogSource.FROM_DOCKER_CONTAINER:
            return self.__get_app_log_from_docker_container(container_name)
        elif log_source == LogSource.FROM_GET_APP_LOG_METHOD:
            return self.containers[container_name].get_app_log()
        else:
            raise Exception("Unexpected log source '%s'" % log_source)

    def __get_app_log_from_docker_container(self, container_name):
        try:
            container = self.client.containers.get(container_name)
        except Exception:
            return 'not started', None

        if b'Segmentation fault' in container.logs():
            logging.warning('Container segfaulted: %s', container.name)
            self.segfault = True

        return container.status, container.logs()

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
        return self.wait_for_app_logs_regex(container_name, self.containers[container_name].get_startup_finished_log_entry(), timeout_seconds, 1)

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
                 and output.count("TCP_MISS") >= output.count("TCP_DENIED"))
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

    def add_test_blob(self, blob_name, content="", with_snapshot=False):
        blob_service_client = BlobServiceClient.from_connection_string(DockerTestCluster.AZURE_CONNECTION_STRING)
        try:
            blob_service_client.create_container("test-container")
        except ResourceExistsError:
            logging.debug('test-container already exists')

        blob_client = blob_service_client.get_blob_client(container="test-container", blob=blob_name)
        blob_client.upload_blob(content)

        if with_snapshot:
            blob_client.create_snapshot()

    def get_blob_and_snapshot_count(self):
        blob_service_client = BlobServiceClient.from_connection_string(DockerTestCluster.AZURE_CONNECTION_STRING)
        container_client = blob_service_client.get_container_client("test-container")
        return len(list(container_client.list_blobs(include=['deleted'])))

    def check_azure_blob_and_snapshot_count(self, blob_and_snapshot_count, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.get_blob_and_snapshot_count() == blob_and_snapshot_count:
                return True
            time.sleep(1)
        return False

    def is_azure_blob_storage_empty(self):
        return self.get_blob_and_snapshot_count() == 0

    def check_azure_blob_storage_is_empty(self, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.is_azure_blob_storage_empty():
                return True
            time.sleep(1)
        return False

    @retry_check()
    def is_s3_bucket_empty(self, container_name):
        (code, output) = self.client.containers.get(container_name).exec_run(["find", "/tmp/", "-type", "d", "-name", "s3mock*"])
        if code != 0:
            return False
        s3_mock_dir = output.decode(self.get_stdout_encoding()).strip()
        (code, output) = self.client.containers.get(container_name).exec_run(["ls", s3_mock_dir + "/test_bucket/"])
        ls_result = output.decode(self.get_stdout_encoding())
        return code == 0 and not ls_result

    @retry_check()
    def check_splunk_event(self, container_name, query):
        (code, output) = self.client.containers.get(container_name).exec_run(["sudo", "/opt/splunk/bin/splunk", "search", query, "-auth", "admin:splunkadmin"])
        if code != 0:
            return False
        return query in output.decode("utf-8")

    @retry_check()
    def check_splunk_event_with_attributes(self, container_name, query, attributes):
        (code, output) = self.client.containers.get(container_name).exec_run(["sudo", "/opt/splunk/bin/splunk", "search", query, "-output", "json", "-auth", "admin:splunkadmin"])
        if code != 0:
            return False
        result_str = output.decode("utf-8")
        result_lines = result_str.splitlines()
        for result_line in result_lines:
            try:
                result_line_json = json.loads(result_line)
            except json.decoder.JSONDecodeError:
                continue
            if "result" not in result_line_json:
                continue
            if "host" in attributes:
                if result_line_json["result"]["host"] != attributes["host"]:
                    continue
            if "source" in attributes:
                if result_line_json["result"]["source"] != attributes["source"]:
                    continue
            if "sourcetype" in attributes:
                if result_line_json["result"]["sourcetype"] != attributes["sourcetype"]:
                    continue
            if "index" in attributes:
                if result_line_json["result"]["index"] != attributes["index"]:
                    continue
            return True
        return False

    def enable_splunk_hec_indexer(self, container_name, hec_name):
        (code, output) = self.client.containers.get(container_name).exec_run(["sudo",
                                                                              "/opt/splunk/bin/splunk", "http-event-collector",
                                                                              "update", hec_name,
                                                                              "-uri", "https://localhost:8089",
                                                                              "-use-ack", "1",
                                                                              "-disabled", "0",
                                                                              "-auth", "admin:splunkadmin"])
        return code == 0

    def enable_splunk_hec_ssl(self, container_name, splunk_cert_pem, splunk_key_pem, root_ca_cert_pem):
        assert self.write_content_to_container(splunk_cert_pem.decode() + splunk_key_pem.decode() + root_ca_cert_pem.decode(), dst=container_name + ':/opt/splunk/etc/auth/splunk_cert.pem')
        assert self.write_content_to_container(root_ca_cert_pem.decode(), dst=container_name + ':/opt/splunk/etc/auth/root_ca.pem')
        (code, output) = self.client.containers.get(container_name).exec_run(["sudo",
                                                                              "/opt/splunk/bin/splunk", "http-event-collector",
                                                                              "update",
                                                                              "-uri", "https://localhost:8089",
                                                                              "-enable-ssl", "1",
                                                                              "-server-cert", "/opt/splunk/etc/auth/splunk_cert.pem",
                                                                              "-ca-cert-file", "/opt/splunk/etc/auth/root_ca.pem",
                                                                              "-require-client-cert", "1",
                                                                              "-auth", "admin:splunkadmin"])
        return code == 0

    @retry_check()
    def check_google_cloud_storage(self, gcs_container_name, content):
        (code, _) = self.client.containers.get(gcs_container_name).exec_run(["grep", "-r", content, "/storage"])
        return code == 0

    @retry_check()
    def is_gcs_bucket_empty(self, container_name):
        (code, output) = self.client.containers.get(container_name).exec_run(["ls", "/storage/test-bucket"])
        return code == 0 and output == b''

    def is_elasticsearch_empty(self, container_name):
        (code, output) = self.client.containers.get(container_name).exec_run(["curl", "-u", "elastic:password", "-k", "-XGET", "https://localhost:9200/_search"])
        return code == 0 and b'"hits":[]' in output

    def create_doc_elasticsearch(self, container_name, index_name, doc_id):
        (code, output) = self.client.containers.get(container_name).exec_run(["/bin/bash", "-c",
                                                                              "curl -u elastic:password -k -XPUT https://localhost:9200/" + index_name + "/_doc/" + doc_id + " -H Content-Type:application/json -d'{\"field1\":\"value1\"}'"])
        return code == 0 and ('"_id":"' + doc_id + '"').encode() in output

    def check_elastic_field_value(self, container_name, index_name, doc_id, field_name, field_value):
        (code, output) = self.client.containers.get(container_name).exec_run(["/bin/bash", "-c",
                                                                              "curl -u elastic:password -k -XGET https://localhost:9200/" + index_name + "/_doc/" + doc_id])
        return code == 0 and (field_name + '":"' + field_value).encode() in output

    def elastic_generate_apikey(self, elastic_container_name):
        (code, output) = self.client.containers.get(elastic_container_name).exec_run(["/bin/bash", "-c",
                                                                                      "curl -u elastic:password -k -XPOST https://localhost:9200/_security/api_key -H Content-Type:application/json -d'{\"name\":\"my-api-key\",\"expiration\":\"1d\",\"role_descriptors\":{\"role-a\": {\"cluster\": [\"all\"],\"index\": [{\"names\": [\"my_index\"],\"privileges\": [\"all\"]}]}}}'"])
        output = output.decode(self.get_stdout_encoding())
        output_lines = output.splitlines()
        result = json.loads(output_lines[-1])
        return result["encoded"]

    def add_elastic_user_to_opensearch(self, container_name):
        (code, output) = self.client.containers.get(container_name).exec_run(["/bin/bash", "-c",
                                                                              'curl -u admin:admin -k -XPUT https://opensearch:9200/_plugins/_security/api/internalusers/elastic -H Content-Type:application/json -d\'{"password":"password","backend_roles":["admin"]}\''])
        return code == 0 and '"status":"CREATED"'.encode() in output

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

    def write_content_to_container(self, content, dst):
        container_name, dst_path = dst.split(':')
        container = self.client.containers.get(container_name)
        with tempfile.TemporaryDirectory() as td:
            with tarfile.open(os.path.join(td, 'content.tar'), mode='w') as tar:
                info = tarfile.TarInfo(name=os.path.basename(dst_path))
                info.size = len(content)
                tar.addfile(info, io.BytesIO(content.encode('utf-8')))
            with open(os.path.join(td, 'content.tar'), 'rb') as data:
                return container.put_archive(os.path.dirname(dst_path), data.read())

    def wait_for_metric_class_on_prometheus(self, metric_class, timeout_seconds):
        return PrometheusChecker().wait_for_metric_class_on_prometheus(metric_class, timeout_seconds)

    def wait_for_processor_metric_on_prometheus(self, metric_class, timeout_seconds, processor_name):
        return PrometheusChecker().wait_for_processor_metric_on_prometheus(metric_class, timeout_seconds, processor_name)
