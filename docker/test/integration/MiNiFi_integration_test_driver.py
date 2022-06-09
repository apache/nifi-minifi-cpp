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

from pydoc import locate

from minifi.core.InputPort import InputPort

from minifi.core.DockerTestCluster import DockerTestCluster

from minifi.validators.EmptyFilesOutPutValidator import EmptyFilesOutPutValidator
from minifi.validators.NoFileOutPutValidator import NoFileOutPutValidator
from minifi.validators.SingleFileOutputValidator import SingleFileOutputValidator
from minifi.validators.MultiFileOutputValidator import MultiFileOutputValidator
from minifi.validators.SingleOrMultiFileOutputValidator import SingleOrMultiFileOutputValidator
from minifi.validators.NoContentCheckFileNumberValidator import NoContentCheckFileNumberValidator
from minifi.validators.NumFileRangeValidator import NumFileRangeValidator
from minifi.validators.SingleJSONFileOutputValidator import SingleJSONFileOutputValidator

from minifi.core.utils import decode_escaped_str


class MiNiFi_integration_test:
    def __init__(self, context):
        self.test_id = context.test_id
        self.cluster = DockerTestCluster(context)

        self.connectable_nodes = []
        # Remote process groups are not connectables
        self.remote_process_groups = []
        self.file_system_observer = None

        self.docker_directory_bindings = context.directory_bindings
        self.cluster.set_directory_bindings(self.docker_directory_bindings.get_directory_bindings(self.test_id), self.docker_directory_bindings.get_data_directories(self.test_id))

    def __del__(self):
        self.cleanup()

    def cleanup(self):
        self.cluster.cleanup()

    def acquire_container(self, name, engine='minifi-cpp', command=None):
        return self.cluster.acquire_container(name, engine, command)

    def wait_for_container_startup_to_finish(self, container_name):
        startup_success = self.cluster.wait_for_startup_log(container_name, 120)
        if not startup_success:
            logging.error("Cluster startup failed for %s", container_name)
            self.cluster.log_app_output()
        return startup_success

    def start_kafka_broker(self):
        self.cluster.acquire_container('kafka-broker', 'kafka-broker')
        self.cluster.deploy('zookeeper')
        self.cluster.deploy('kafka-broker')
        assert self.wait_for_container_startup_to_finish('kafka-broker')

    def start_splunk(self):
        self.cluster.acquire_container('splunk', 'splunk')
        self.cluster.deploy('splunk')
        assert self.wait_for_container_startup_to_finish('splunk')
        assert self.cluster.enable_splunk_hec_indexer('splunk', 'splunk_hec_token')

    def start(self):
        logging.info("MiNiFi_integration_test start")
        self.cluster.deploy_flow()
        for container_name in self.cluster.containers:
            assert self.wait_for_container_startup_to_finish(container_name)

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

    def add_test_data(self, path, test_data, file_name=str(uuid.uuid4())):
        test_data = decode_escaped_str(test_data)
        self.docker_directory_bindings.put_file_to_docker_path(self.test_id, path, file_name, test_data.encode('utf-8'))

    def put_test_resource(self, file_name, contents):
        self.docker_directory_bindings.put_test_resource(self.test_id, file_name, contents)

    def rm_out_child(self):
        self.docker_directory_bindings.rm_out_child(self.test_id)

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

    def check_for_at_least_one_file_with_content_generated(self, content, timeout_seconds):
        output_validator = SingleOrMultiFileOutputValidator(decode_escaped_str(content))
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator)

    def check_for_num_files_generated(self, num_flowfiles, timeout_seconds):
        output_validator = NoContentCheckFileNumberValidator(num_flowfiles)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator, max(1, num_flowfiles))

    def check_for_num_file_range_generated(self, min_files, max_files, wait_time_in_seconds):
        output_validator = NumFileRangeValidator(min_files, max_files)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output_after_time_period(wait_time_in_seconds, output_validator)

    def check_for_an_empty_file_generated(self, timeout_seconds):
        output_validator = EmptyFilesOutPutValidator()
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output(timeout_seconds, output_validator, 1)

    def __check_output_after_time_period(self, wait_time_in_seconds, output_validator):
        time.sleep(wait_time_in_seconds)
        self.__validate(output_validator)

    def __check_output(self, timeout_seconds, output_validator, max_files=0):
        result = self.file_system_observer.validate_output(timeout_seconds, output_validator, max_files)
        self.cluster.log_app_output()
        assert not self.cluster.segfault_happened()
        assert result

    def __validate(self, validator):
        self.cluster.log_app_output()
        assert not self.cluster.segfault_happened()
        assert validator.validate()

    def check_s3_server_object_data(self, s3_container_name, object_data):
        assert self.cluster.check_s3_server_object_data(s3_container_name, object_data)

    def check_s3_server_object_metadata(self, s3_container_name, content_type):
        assert self.cluster.check_s3_server_object_metadata(s3_container_name, content_type)

    def check_empty_s3_bucket(self, s3_container_name):
        assert self.cluster.is_s3_bucket_empty(s3_container_name)

    def check_http_proxy_access(self, http_proxy_container_name, url):
        assert self.cluster.check_http_proxy_access(http_proxy_container_name, url)

    def check_azure_storage_server_data(self, azure_container_name, object_data):
        assert self.cluster.check_azure_storage_server_data(azure_container_name, object_data)

    def wait_for_kafka_consumer_to_be_registered(self, kafka_container_name):
        assert self.cluster.wait_for_kafka_consumer_to_be_registered(kafka_container_name)

    def check_splunk_event(self, splunk_container_name, query):
        assert self.cluster.check_splunk_event(splunk_container_name, query)

    def check_splunk_event_with_attributes(self, splunk_container_name, query, attributes):
        assert self.cluster.check_splunk_event_with_attributes(splunk_container_name, query, attributes)

    def check_google_cloud_storage(self, gcs_container_name, content):
        assert self.cluster.check_google_cloud_storage(gcs_container_name, content)

    def check_empty_gcs_bucket(self, gcs_container_name):
        assert self.cluster.is_gcs_bucket_empty(gcs_container_name)

    def check_minifi_log_contents(self, line, timeout_seconds=60, count=1):
        self.check_container_log_contents("minifi-cpp", line, timeout_seconds, count)

    def check_minifi_log_matches_regex(self, regex, timeout_seconds=60, count=1):
        for container in self.cluster.containers.values():
            if container.get_engine() == "minifi-cpp":
                line_found = self.cluster.wait_for_app_logs_regex(container.get_name(), regex, timeout_seconds, count)
                if line_found:
                    return
        assert False

    def check_container_log_contents(self, container_engine, line, timeout_seconds=60, count=1):
        for container in self.cluster.containers.values():
            if container.get_engine() == container_engine:
                line_found = self.cluster.wait_for_app_logs(container.get_name(), line, timeout_seconds, count)
                if line_found:
                    return
        assert False

    def check_minifi_log_does_not_contain(self, line, wait_time_seconds):
        time.sleep(wait_time_seconds)
        for container in self.cluster.containers.values():
            if container.get_engine() == "minifi-cpp":
                _, logs = self.cluster.get_app_log(container.get_name())
                if logs is not None and 1 <= logs.decode("utf-8").count(line):
                    assert False

    def check_query_results(self, postgresql_container_name, query, number_of_rows, timeout_seconds):
        assert self.cluster.check_query_results(postgresql_container_name, query, number_of_rows, timeout_seconds)

    def check_container_log_matches_regex(self, container_name, log_pattern, timeout_seconds, count=1):
        assert self.cluster.wait_for_app_logs_regex(container_name, log_pattern, timeout_seconds, count)

    def add_test_blob(self, blob_name, content, with_snapshot):
        self.cluster.add_test_blob(blob_name, content, with_snapshot)

    def check_azure_blob_storage_is_empty(self, timeout_seconds):
        assert self.cluster.check_azure_blob_storage_is_empty(timeout_seconds)

    def check_azure_blob_and_snapshot_count(self, blob_and_snapshot_count, timeout_seconds):
        assert self.cluster.check_azure_blob_and_snapshot_count(blob_and_snapshot_count, timeout_seconds)

    def check_metric_class_on_prometheus(self, metric_class, timeout_seconds):
        assert self.cluster.wait_for_metric_class_on_prometheus(metric_class, timeout_seconds)

    def check_processor_metric_on_prometheus(self, metric_class, timeout_seconds, processor_name):
        assert self.cluster.wait_for_processor_metric_on_prometheus(metric_class, timeout_seconds, processor_name)
