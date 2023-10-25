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
import time
import uuid

import OpenSSL.crypto

from pydoc import locate

from ssl_utils.SSL_cert_utils import make_self_signed_cert, make_cert_without_extended_usage, make_server_cert
from minifi.core.InputPort import InputPort

from cluster.DockerTestCluster import DockerTestCluster

from minifi.validators.OutputValidator import OutputValidator
from minifi.validators.EmptyFilesOutPutValidator import EmptyFilesOutPutValidator
from minifi.validators.NoFileOutPutValidator import NoFileOutPutValidator
from minifi.validators.SingleFileOutputValidator import SingleFileOutputValidator
from minifi.validators.MultiFileOutputValidator import MultiFileOutputValidator
from minifi.validators.SingleOrMultiFileOutputValidator import SingleOrMultiFileOutputValidator
from minifi.validators.SingleOrMultiFileOutputRegexValidator import SingleOrMultiFileOutputRegexValidator
from minifi.validators.NoContentCheckFileNumberValidator import NoContentCheckFileNumberValidator
from minifi.validators.NumFileRangeValidator import NumFileRangeValidator
from minifi.validators.NumFileRangeAndFileSizeValidator import NumFileRangeAndFileSizeValidator
from minifi.validators.SingleJSONFileOutputValidator import SingleJSONFileOutputValidator
from utils import decode_escaped_str, get_minifi_pid, get_peak_memory_usage


class MiNiFi_integration_test:
    def __init__(self, context, feature_id: str):
        self.feature_id = feature_id
        self.cluster = DockerTestCluster(context, feature_id=feature_id)

        self.connectable_nodes = []
        # Remote process groups are not connectables
        self.remote_process_groups = []
        self.file_system_observer = None
        self.test_file_hash = None

        self.docker_directory_bindings = context.directory_bindings
        self.cluster.set_directory_bindings(self.docker_directory_bindings.get_directory_bindings(self.feature_id), self.docker_directory_bindings.get_data_directories(self.feature_id))
        self.root_ca_cert, self.root_ca_key = make_self_signed_cert("root CA")

        minifi_client_cert, minifi_client_key = make_cert_without_extended_usage(common_name=f"minifi-cpp-flow-{self.feature_id}",
                                                                                 ca_cert=self.root_ca_cert,
                                                                                 ca_key=self.root_ca_key)
        minifi_server_cert, minifi_server_key = make_server_cert(common_name=f"server-{self.feature_id}",
                                                                 ca_cert=self.root_ca_cert,
                                                                 ca_key=self.root_ca_key)
        self_signed_server_cert, self_signed_server_key = make_self_signed_cert(f"server-{self.feature_id}")

        self.put_test_resource('root_ca.crt',
                               OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM,
                                                               cert=self.root_ca_cert))
        self.put_test_resource("system_certs_dir/ca-root-nss.crt",
                               OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM,
                                                               cert=self.root_ca_cert))
        self.put_test_resource('minifi_client.crt',
                               OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM,
                                                               cert=minifi_client_cert))
        self.put_test_resource('minifi_client.key',
                               OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM,
                                                              pkey=minifi_client_key))
        self.put_test_resource('minifi_server.crt',
                               OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM,
                                                               cert=minifi_server_cert)
                               + OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM,
                                                                pkey=minifi_server_key))
        self.put_test_resource('self_signed_server.crt',
                               OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM,
                                                               cert=self_signed_server_cert)
                               + OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM,
                                                                pkey=self_signed_server_key))
        self.put_test_resource('minifi_merged_cert.crt',
                               OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM,
                                                               cert=minifi_client_cert)
                               + OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM,
                                                                pkey=minifi_client_key))

    def get_container_name_with_postfix(self, container_name: str):
        return self.cluster.container_store.get_container_name_with_postfix(container_name)

    def cleanup(self):
        self.cluster.cleanup()
        if self.file_system_observer:
            self.file_system_observer.observer.unschedule_all()

    def acquire_container(self, context, name, engine='minifi-cpp', command=None):
        return self.cluster.acquire_container(context=context, name=name, engine=engine, command=command)

    def start_kafka_broker(self, context):
        self.cluster.acquire_container(context=context, name='kafka-broker', engine='kafka-broker')
        self.cluster.deploy_container(name='zookeeper')
        self.cluster.deploy_container(name='kafka-broker')
        assert self.cluster.wait_for_container_startup_to_finish('kafka-broker')

    def start_splunk(self, context):
        self.cluster.acquire_container(context=context, name='splunk', engine='splunk')
        self.cluster.deploy_container(name='splunk')
        assert self.cluster.wait_for_container_startup_to_finish('splunk')
        assert self.cluster.enable_splunk_hec_indexer('splunk', 'splunk_hec_token')

    def start_elasticsearch(self, context):
        self.cluster.acquire_container(context=context, name='elasticsearch', engine='elasticsearch')
        self.cluster.deploy_container('elasticsearch')
        assert self.cluster.wait_for_container_startup_to_finish('elasticsearch')

    def start_opensearch(self, context):
        self.cluster.acquire_container(context=context, name='opensearch', engine='opensearch')
        self.cluster.deploy_container('opensearch')
        assert self.cluster.wait_for_container_startup_to_finish('opensearch')

    def start_minifi_c2_server(self, context):
        self.cluster.acquire_container(context=context, name="minifi-c2-server", engine="minifi-c2-server")
        self.cluster.deploy_container('minifi-c2-server')
        assert self.cluster.wait_for_container_startup_to_finish('minifi-c2-server')

    def start(self, container_name=None):
        if container_name is not None:
            logging.info("Starting container %s", container_name)
            self.cluster.deploy_container(container_name)
            assert self.cluster.wait_for_container_startup_to_finish(container_name)
            return
        logging.info("MiNiFi_integration_test start")
        self.cluster.deploy_all()
        assert self.cluster.wait_for_all_containers_to_finish_startup()

    def stop(self, container_name):
        logging.info("Stopping container %s", container_name)
        self.cluster.stop_container(container_name)

    def kill(self, container_name):
        logging.info("Killing container %s", container_name)
        self.cluster.kill_container(container_name)

    def restart(self, container_name):
        logging.info("Restarting container %s", container_name)
        self.cluster.restart_container(container_name)

    def add_node(self, processor):
        if processor.get_name() in (elem.get_name() for elem in self.connectable_nodes):
            raise Exception("Trying to register processor with an already registered name: \"%s\"" % processor.get_name())
        self.connectable_nodes.append(processor)

    def get_or_create_node_by_name(self, node_name):
        node = self.get_node_by_name(node_name)
        if node is None:
            if node_name == "RemoteProcessGroup":
                raise Exception("Trying to register RemoteProcessGroup without an input port or address.")
            node = locate("minifi.processors." + node_name + "." + node_name)()
            node.set_name(node_name)
            self.add_node(node)
        return node

    def get_node_by_name(self, name):
        for node in self.connectable_nodes:
            if name == node.get_name():
                return node
        raise Exception("Trying to fetch unknown node: \"%s\"" % name)

    def add_remote_process_group(self, remote_process_group):
        if remote_process_group.get_name() in (elem.get_name() for elem in self.remote_process_groups):
            raise Exception("Trying to register remote_process_group with an already registered name: \"%s\"" % remote_process_group.get_name())
        self.remote_process_groups.append(remote_process_group)

    def get_remote_process_group_by_name(self, name):
        for node in self.remote_process_groups:
            if name == node.get_name():
                return node
        raise Exception("Trying to fetch unknow node: \"%s\"" % name)

    @staticmethod
    def generate_input_port_for_remote_process_group(remote_process_group, name):
        input_port_node = InputPort(name, remote_process_group)
        # Generate an MD5 hash unique to the remote process group id
        input_port_node.set_uuid(uuid.uuid3(remote_process_group.get_uuid(), "input_port"))
        return input_port_node

    def add_test_data(self, path, test_data, file_name=None):
        if file_name is None:
            file_name = str(uuid.uuid4())
        test_data = decode_escaped_str(test_data)
        self.docker_directory_bindings.put_file_to_docker_path(self.feature_id, path, file_name, test_data.encode('utf-8'))

    def add_random_test_data(self, path: str, size: int, file_name: str = None):
        if file_name is None:
            file_name = str(uuid.uuid4())
        self.test_file_hash = self.docker_directory_bindings.put_random_file_to_docker_path(self.feature_id, path, file_name, size)

    def put_test_resource(self, file_name, contents):
        self.docker_directory_bindings.put_test_resource(self.feature_id, file_name, contents)

    def rm_out_child(self):
        self.docker_directory_bindings.rm_out_child(self.feature_id)

    def add_file_system_observer(self, file_system_observer):
        self.file_system_observer = file_system_observer

    def check_for_no_files_generated(self, wait_time_in_seconds):
        output_validator = NoFileOutPutValidator()
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output_after_time_period(wait_time_in_seconds, output_validator)

    def check_for_single_file_with_content_generated(self, content, timeout_seconds):
        output_validator = SingleFileOutputValidator(decode_escaped_str(content))
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator, 1)

    def check_for_single_json_file_with_content_generated(self, content, timeout_seconds):
        output_validator = SingleJSONFileOutputValidator(content)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator, 1)

    def check_for_multiple_files_generated(self, file_count, timeout_seconds, expected_content=[]):
        output_validator = MultiFileOutputValidator(file_count, [decode_escaped_str(content) for content in expected_content])
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator, file_count)

    def check_for_at_least_one_file_with_matching_content(self, regex, timeout_seconds):
        output_validator = SingleOrMultiFileOutputRegexValidator(regex)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator)

    def check_for_at_least_one_file_with_content_generated(self, content, timeout_seconds):
        output_validator = SingleOrMultiFileOutputValidator(decode_escaped_str(content))
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator)

    def check_for_num_files_generated(self, num_flowfiles, timeout_seconds):
        output_validator = NoContentCheckFileNumberValidator(num_flowfiles)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator, max(1, num_flowfiles))

    def check_for_num_file_range_generated_after_wait(self, min_files: int, max_files: int, wait_time_in_seconds: int):
        output_validator = NumFileRangeValidator(min_files, max_files)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output_after_time_period(wait_time_in_seconds, output_validator)

    def check_for_num_file_range_generated_with_timeout(self, min_files: int, max_files: int, timeout_in_seconds: int):
        output_validator = NumFileRangeValidator(min_files, max_files)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output_over_time_period(timeout_in_seconds, output_validator)

    def check_for_num_file_range_and_min_size_generated(self, min_files: int, max_files: int, min_size: int, wait_time_in_seconds: int):
        output_validator = NumFileRangeAndFileSizeValidator(min_files, max_files, min_size)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output_over_time_period(wait_time_in_seconds, output_validator)

    def check_for_an_empty_file_generated(self, timeout_seconds):
        output_validator = EmptyFilesOutPutValidator()
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator, 1)

    def __check_output_after_time_period(self, wait_time_in_seconds, output_validator):
        time.sleep(wait_time_in_seconds)
        self.__validate(output_validator)

    def __check_output_over_time_period(self, wait_time_in_seconds: int, output_validator: OutputValidator):
        start_time = time.perf_counter()
        while True:
            assert not self.cluster.segfault_happened() or self.cluster.log_app_output()
            if output_validator.validate():
                return
            time.sleep(1)
            if wait_time_in_seconds < (time.perf_counter() - start_time):
                break
        assert output_validator.validate() or self.cluster.log_app_output()

    def __check_output(self, timeout_seconds, output_validator, max_files=0):
        result = self.file_system_observer.validate_output(timeout_seconds, output_validator, max_files)
        assert not self.cluster.segfault_happened() or self.cluster.log_app_output()
        assert result or self.cluster.log_app_output()

    def __validate(self, validator):
        assert not self.cluster.segfault_happened() or self.cluster.log_app_output()
        assert validator.validate() or self.cluster.log_app_output()

    def check_s3_server_object_data(self, s3_container_name, object_data):
        assert self.cluster.check_s3_server_object_data(s3_container_name, object_data) or self.cluster.log_app_output()

    def check_s3_server_large_object_data(self, s3_container_name: str):
        assert self.cluster.check_s3_server_object_hash(s3_container_name, self.test_file_hash) or self.cluster.log_app_output()

    def check_s3_server_object_metadata(self, s3_container_name, content_type):
        assert self.cluster.check_s3_server_object_metadata(s3_container_name, content_type) or self.cluster.log_app_output()

    def check_empty_s3_bucket(self, s3_container_name):
        assert self.cluster.is_s3_bucket_empty(s3_container_name) or self.cluster.log_app_output()

    def check_http_proxy_access(self, http_proxy_container_name, url):
        assert self.cluster.check_http_proxy_access(http_proxy_container_name, url) or self.cluster.log_app_output()

    def check_azure_storage_server_data(self, azure_container_name, object_data):
        assert self.cluster.check_azure_storage_server_data(azure_container_name, object_data) or self.cluster.log_app_output()

    def wait_for_kafka_consumer_to_be_registered(self, kafka_container_name):
        assert self.cluster.wait_for_kafka_consumer_to_be_registered(kafka_container_name) or self.cluster.log_app_output()

    def check_splunk_event(self, splunk_container_name, query):
        assert self.cluster.check_splunk_event(splunk_container_name, query) or self.cluster.log_app_output()

    def check_splunk_event_with_attributes(self, splunk_container_name, query, attributes):
        assert self.cluster.check_splunk_event_with_attributes(splunk_container_name, query, attributes) or self.cluster.log_app_output()

    def check_google_cloud_storage(self, gcs_container_name, content):
        assert self.cluster.check_google_cloud_storage(gcs_container_name, content) or self.cluster.log_app_output()

    def check_empty_gcs_bucket(self, gcs_container_name):
        assert self.cluster.is_gcs_bucket_empty(gcs_container_name) or self.cluster.log_app_output()

    def check_empty_elastic(self, elastic_container_name):
        assert self.cluster.is_elasticsearch_empty(elastic_container_name) or self.cluster.log_app_output()

    def elastic_generate_apikey(self, elastic_container_name):
        return self.cluster.elastic_generate_apikey(elastic_container_name) or self.cluster.log_app_output()

    def create_doc_elasticsearch(self, elastic_container_name, index_name, doc_id):
        assert self.cluster.create_doc_elasticsearch(elastic_container_name, index_name, doc_id) or self.cluster.log_app_output()

    def check_elastic_field_value(self, elastic_container_name, index_name, doc_id, field_name, field_value):
        assert self.cluster.check_elastic_field_value(elastic_container_name, index_name, doc_id, field_name, field_value) or self.cluster.log_app_output()

    def add_elastic_user_to_opensearch(self, container_name):
        assert self.cluster.add_elastic_user_to_opensearch(container_name) or self.cluster.log_app_output()

    def check_minifi_log_contents(self, line, timeout_seconds=60, count=1):
        self.check_container_log_contents("minifi-cpp", line, timeout_seconds, count)

    def check_minifi_log_matches_regex(self, regex, timeout_seconds=60, count=1):
        assert self.cluster.check_minifi_log_matches_regex(regex, timeout_seconds, count) or self.cluster.log_app_output()

    def check_container_log_contents(self, container_engine, line, timeout_seconds=60, count=1):
        assert self.cluster.check_container_log_contents(container_engine, line, timeout_seconds, count) or self.cluster.log_app_output()

    def check_minifi_log_does_not_contain(self, line, wait_time_seconds):
        assert self.cluster.check_minifi_log_does_not_contain(line, wait_time_seconds) or self.cluster.log_app_output()

    def check_query_results(self, postgresql_container_name, query, number_of_rows, timeout_seconds):
        assert self.cluster.check_query_results(postgresql_container_name, query, number_of_rows, timeout_seconds) or self.cluster.log_app_output()

    def check_container_log_matches_regex(self, container_name, log_pattern, timeout_seconds, count=1):
        assert self.cluster.wait_for_app_logs_regex(container_name, log_pattern, timeout_seconds, count) or self.cluster.log_app_output()

    def add_test_blob(self, blob_name, content, with_snapshot):
        self.cluster.add_test_blob(blob_name, content, with_snapshot)

    def check_azure_blob_storage_is_empty(self, timeout_seconds):
        assert self.cluster.check_azure_blob_storage_is_empty(timeout_seconds) or self.cluster.log_app_output()

    def check_azure_blob_and_snapshot_count(self, blob_and_snapshot_count, timeout_seconds):
        assert self.cluster.check_azure_blob_and_snapshot_count(blob_and_snapshot_count, timeout_seconds) or self.cluster.log_app_output()

    def check_metric_class_on_prometheus(self, metric_class, timeout_seconds):
        assert self.cluster.wait_for_metric_class_on_prometheus(metric_class, timeout_seconds) or self.cluster.log_app_output()

    def check_processor_metric_on_prometheus(self, metric_class, timeout_seconds, processor_name):
        assert self.cluster.wait_for_processor_metric_on_prometheus(metric_class, timeout_seconds, processor_name) or self.cluster.log_app_output()

    def check_if_peak_memory_usage_exceeded(self, minimum_peak_memory_usage: int, timeout_seconds: int) -> None:
        assert self.cluster.wait_for_peak_memory_usage_to_exceed(minimum_peak_memory_usage, timeout_seconds) or self.cluster.log_app_output()

    def check_if_memory_usage_is_below(self, maximum_memory_usage: int, timeout_seconds: int) -> None:
        assert self.cluster.wait_for_memory_usage_to_drop_below(maximum_memory_usage, timeout_seconds) or self.cluster.log_app_output()

    def check_memory_usage_compared_to_peak(self, peak_multiplier: float, timeout_seconds: int) -> None:
        peak_memory = get_peak_memory_usage(get_minifi_pid())
        assert (peak_memory is not None) or self.cluster.log_app_output()
        assert (1.0 > peak_multiplier > 0.0) or self.cluster.log_app_output()
        assert self.cluster.wait_for_memory_usage_to_drop_below(peak_memory * peak_multiplier, timeout_seconds) or self.cluster.log_app_output()

    def enable_provenance_repository_in_minifi(self):
        self.cluster.enable_provenance_repository_in_minifi()

    def enable_c2_in_minifi(self):
        self.cluster.enable_c2_in_minifi()

    def enable_c2_with_ssl_in_minifi(self):
        self.cluster.enable_c2_with_ssl_in_minifi()

    def fetch_flow_config_from_c2_url_in_minifi(self):
        self.cluster.fetch_flow_config_from_c2_url_in_minifi()

    def set_ssl_context_properties_in_minifi(self):
        self.cluster.set_ssl_context_properties_in_minifi()

    def enable_prometheus_in_minifi(self):
        self.cluster.enable_prometheus_in_minifi()

    def enable_prometheus_with_ssl_in_minifi(self):
        self.cluster.enable_prometheus_with_ssl_in_minifi()

    def enable_splunk_hec_ssl(self, container_name, splunk_cert_pem, splunk_key_pem, root_ca_cert_pem):
        self.cluster.enable_splunk_hec_ssl(container_name, splunk_cert_pem, splunk_key_pem, root_ca_cert_pem)

    def enable_sql_in_minifi(self):
        self.cluster.enable_sql_in_minifi()

    def set_yaml_in_minifi(self):
        self.cluster.set_yaml_in_minifi()

    def set_controller_socket_properties_in_minifi(self):
        self.cluster.set_controller_socket_properties_in_minifi()

    def update_flow_config_through_controller(self, container_name: str):
        self.cluster.update_flow_config_through_controller(container_name)

    def check_minifi_controller_updated_config_is_persisted(self, container_name: str):
        assert self.cluster.check_minifi_controller_updated_config_is_persisted(container_name) or self.cluster.log_app_output()

    def stop_component_through_controller(self, component: str, container_name: str):
        self.cluster.stop_component_through_controller(component, container_name)

    def start_component_through_controller(self, component: str, container_name: str):
        self.cluster.start_component_through_controller(component, container_name)

    def check_component_not_running_through_controller(self, component: str, container_name: str):
        assert self.cluster.check_component_not_running_through_controller(component, container_name) or self.cluster.log_app_output()

    def check_component_running_through_controller(self, component: str, container_name: str):
        assert self.cluster.check_component_running_through_controller(component, container_name) or self.cluster.log_app_output()

    def connection_found_through_controller(self, connection: str, container_name: str):
        assert self.cluster.connection_found_through_controller(connection, container_name) or self.cluster.log_app_output()

    def check_connections_full_through_controller(self, connection_count: int, container_name: str):
        assert self.cluster.check_connections_full_through_controller(connection_count, container_name) or self.cluster.log_app_output()

    def check_connection_size_through_controller(self, connection: str, size: int, max_size: int, container_name: str):
        assert self.cluster.check_connection_size_through_controller(connection, size, max_size, container_name) or self.cluster.log_app_output()

    def manifest_can_be_retrieved_through_minifi_controller(self, container_name: str):
        assert self.cluster.manifest_can_be_retrieved_through_minifi_controller(container_name) or self.cluster.log_app_output()

    def enable_log_metrics_publisher_in_minifi(self):
        self.cluster.enable_log_metrics_publisher_in_minifi()

    def debug_bundle_can_be_retrieved_through_minifi_controller(self, container_name: str):
        assert self.cluster.debug_bundle_can_be_retrieved_through_minifi_controller(container_name) or self.cluster.log_app_output()
