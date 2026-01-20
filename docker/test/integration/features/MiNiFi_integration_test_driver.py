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
import time
import uuid

from pydoc import locate
from minifi.core.InputPort import InputPort
from minifi.core.OutputPort import OutputPort
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
        self.cluster.set_directory_bindings(self.docker_directory_bindings.get_directory_bindings(), self.docker_directory_bindings.get_data_directories())

    def get_container_name_with_postfix(self, container_name: str):
        return self.cluster.container_store.get_container_name_with_postfix(container_name)

    def cleanup(self):
        self.cluster.cleanup()
        if self.file_system_observer:
            self.file_system_observer.observer.unschedule_all()

    def acquire_container(self, context, name, engine='minifi-cpp', command=None):
        return self.cluster.acquire_container(context=context, name=name, engine=engine, command=command)

    def acquire_transient_minifi(self, context, name, engine='minifi-cpp'):
        return self.cluster.acquire_transient_minifi(context=context, name=name, engine=engine)

    def start_nifi(self, context):
        self.cluster.acquire_container(context=context, name='nifi', engine='nifi')
        self.cluster.deploy_container('nifi')
        assert self.cluster.wait_for_container_startup_to_finish('nifi') or self.cluster.log_app_output()

    def start(self, container_name=None):
        if container_name is not None:
            logging.info("Starting container %s", container_name)
            self.cluster.deploy_container(container_name)
            assert self.cluster.wait_for_container_startup_to_finish(container_name) or self.cluster.log_app_output()
            return
        logging.info("MiNiFi_integration_test start")
        self.cluster.deploy_all()
        assert self.cluster.wait_for_all_containers_to_finish_startup() or self.cluster.log_app_output()

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
        raise Exception("Trying to fetch unknown node: \"%s\"" % name)

    @staticmethod
    def generate_input_port_for_remote_process_group(remote_process_group, name, use_compression=False):
        input_port_node = InputPort(name, remote_process_group)
        # Generate an MD5 hash unique to the remote process group id
        input_port_node.set_uuid(uuid.uuid3(remote_process_group.get_uuid(), "input_port"))
        input_port_node.set_use_compression(use_compression)
        return input_port_node

    @staticmethod
    def generate_output_port_for_remote_process_group(remote_process_group, name, use_compression=False):
        output_port_node = OutputPort(name, remote_process_group)
        # Generate an MD5 hash unique to the remote process group id
        output_port_node.set_uuid(uuid.uuid3(remote_process_group.get_uuid(), "output_port"))
        output_port_node.set_use_compression(use_compression)
        return output_port_node

    def add_test_data(self, path, test_data, file_name=None):
        if file_name is None:
            file_name = str(uuid.uuid4())
        test_data = decode_escaped_str(test_data)
        self.docker_directory_bindings.put_file_to_docker_path(path, file_name, test_data.encode('utf-8'))

    def add_random_test_data(self, path: str, size: int, file_name: str = None):
        if file_name is None:
            file_name = str(uuid.uuid4())
        self.test_file_hash = self.docker_directory_bindings.put_random_file_to_docker_path(path, file_name, size)

    def put_test_resource(self, file_name, contents):
        self.docker_directory_bindings.put_test_resource(file_name, contents)

    def get_test_resource_path(self, file_name):
        return self.docker_directory_bindings.get_test_resource_path(file_name)

    def add_file_system_observer(self, file_system_observer):
        self.file_system_observer = file_system_observer

    def check_for_no_files_generated(self, wait_time_in_seconds):
        output_validator = NoFileOutPutValidator()
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.__check_output_after_time_period(wait_time_in_seconds, output_validator)

    def check_for_no_files_generated_in_subdir(self, wait_time_in_seconds, subdir):
        output_validator = NoFileOutPutValidator()
        output_validator.set_output_dir(self.file_system_observer.get_output_dir() + "/" + subdir)
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

    def check_subdirectory(self, sub_directory: str, expected_contents: list, timeout: int, interval: float = 1.0) -> bool:
        logging.info("check_directory")
        start_time = time.time()
        expected_contents.sort()
        while time.time() - start_time < timeout:
            try:
                current_contents = []
                directory = self.file_system_observer.get_output_dir() + "/" + sub_directory
                current_files = os.listdir(directory)
                for file in current_files:
                    file_path = os.path.join(directory, file)
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                        current_contents.append(content)
                current_contents.sort()

                if current_contents == expected_contents:
                    logging.info("subdir checks out")
                    return True
                logging.info(f"expected: {expected_contents} vs actual {current_contents}")

            except Exception as e:
                print(f"Error checking directory: {e}")

            time.sleep(interval)

        return False

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

    def check_http_proxy_access(self, http_proxy_container_name, url):
        assert self.cluster.check_http_proxy_access(http_proxy_container_name, url) or self.cluster.log_app_output()

    def check_azure_storage_server_data(self, azure_container_name, object_data):
        assert self.cluster.check_azure_storage_server_data(azure_container_name, object_data) or self.cluster.log_app_output()

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

    def check_all_prometheus_metric_types_are_defined_once(self):
        assert self.cluster.verify_all_metric_types_are_defined_once() or self.cluster.log_app_output()

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

    def enable_sql_in_minifi(self):
        self.cluster.enable_sql_in_minifi()

    def use_nifi_python_processors_with_system_python_packages_installed_in_minifi(self):
        self.cluster.use_nifi_python_processors_with_system_python_packages_installed_in_minifi()

    def use_nifi_python_processors_with_virtualenv_in_minifi(self):
        self.cluster.use_nifi_python_processors_with_virtualenv_in_minifi()

    def use_nifi_python_processors_with_virtualenv_packages_installed_in_minifi(self):
        self.cluster.use_nifi_python_processors_with_virtualenv_packages_installed_in_minifi()

    def remove_python_requirements_txt_in_minifi(self):
        self.cluster.remove_python_requirements_txt_in_minifi()

    def use_nifi_python_processors_without_dependencies_in_minifi(self):
        self.cluster.use_nifi_python_processors_without_dependencies_in_minifi()

    def set_yaml_in_minifi(self):
        self.cluster.set_yaml_in_minifi()

    def set_json_in_minifi(self):
        self.cluster.set_json_in_minifi()

    def llama_model_is_downloaded_in_minifi(self):
        self.cluster.llama_model_is_downloaded_in_minifi()

    def enable_log_metrics_publisher_in_minifi(self):
        self.cluster.enable_log_metrics_publisher_in_minifi()

    def enable_example_minifi_python_processors(self):
        self.cluster.enable_example_minifi_python_processors()

    def enable_openssl_fips_mode_in_minifi(self):
        self.cluster.enable_openssl_fips_mode_in_minifi()

    def disable_openssl_fips_mode_in_minifi(self):
        self.cluster.disable_openssl_fips_mode_in_minifi()

    def set_value_on_plc_with_modbus(self, container_name, modbus_cmd):
        assert self.cluster.set_value_on_plc_with_modbus(container_name, modbus_cmd)

    def enable_ssl_in_nifi(self):
        self.cluster.enable_ssl_in_nifi()
