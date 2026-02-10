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
import re

from .LogSource import LogSource
from .ContainerStore import ContainerStore
from .DockerCommunicator import DockerCommunicator
from .checkers.AzureChecker import AzureChecker
from .checkers.PostgresChecker import PostgresChecker
from .checkers.ModbusChecker import ModbusChecker
from utils import get_peak_memory_usage, get_minifi_pid, get_memory_usage


class DockerTestCluster:
    def __init__(self, context, feature_id):
        self.segfault = False
        self.vols = {}
        self.container_communicator = DockerCommunicator()
        self.container_store = ContainerStore(self.container_communicator.create_docker_network(feature_id), context.image_store, feature_id=feature_id)
        self.azure_checker = AzureChecker(self.container_communicator)
        self.postgres_checker = PostgresChecker(self.container_communicator)
        self.modbus_checker = ModbusChecker(self.container_communicator)

    def cleanup(self):
        self.container_store.cleanup()

    def set_directory_bindings(self, volumes, data_directories):
        self.container_store.set_directory_bindings(volumes, data_directories)

    def acquire_container(self, context, name: str, engine: str = 'minifi-cpp', command=None):
        return self.container_store.acquire_container(context=context, container_name=name, engine=engine, command=command)

    def acquire_transient_minifi(self, context, name: str, engine: str = 'minifi-cpp'):
        return self.container_store.acquire_transient_minifi(context=context, container_name=name, engine=engine)

    def deploy_container(self, name):
        self.container_store.deploy_container(name)

    def deploy_all(self):
        self.container_store.deploy_all()

    def stop_container(self, container_name):
        self.container_store.stop_container(container_name)

    def kill_container(self, container_name):
        self.container_store.kill_container(container_name)

    def restart_container(self, container_name):
        self.container_store.restart_container(container_name)

    def enable_provenance_repository_in_minifi(self):
        self.container_store.enable_provenance_repository_in_minifi()

    def enable_c2_in_minifi(self):
        self.container_store.enable_c2_in_minifi()

    def enable_c2_with_ssl_in_minifi(self):
        self.container_store.enable_c2_with_ssl_in_minifi()

    def fetch_flow_config_from_c2_url_in_minifi(self):
        self.container_store.fetch_flow_config_from_c2_url_in_minifi()

    def set_ssl_context_properties_in_minifi(self):
        self.container_store.set_ssl_context_properties_in_minifi()

    def enable_openssl_fips_mode_in_minifi(self):
        self.container_store.enable_openssl_fips_mode_in_minifi()

    def disable_openssl_fips_mode_in_minifi(self):
        self.container_store.disable_openssl_fips_mode_in_minifi()

    def enable_sql_in_minifi(self):
        self.container_store.enable_sql_in_minifi()

    def set_yaml_in_minifi(self):
        self.container_store.set_yaml_in_minifi()

    def set_json_in_minifi(self):
        self.container_store.set_json_in_minifi()

    def enable_log_metrics_publisher_in_minifi(self):
        self.container_store.enable_log_metrics_publisher_in_minifi()

    def llama_model_is_downloaded_in_minifi(self):
        self.container_store.llama_model_is_downloaded_in_minifi()

    def get_app_log(self, container_name):
        container_name = self.container_store.get_container_name_with_postfix(container_name)
        log_source = self.container_store.log_source(container_name)
        if log_source == LogSource.FROM_DOCKER_CONTAINER:
            return self.container_communicator.get_app_log_from_docker_container(container_name)
        elif log_source == LogSource.FROM_GET_APP_LOG_METHOD:
            return self.container_store.get_app_log(container_name)
        else:
            raise Exception("Unexpected log source '%s'" % log_source)

    def __wait_for_app_logs_impl(self, container_name, log_entry, timeout_seconds, count, use_regex):
        wait_start_time = time.perf_counter()
        while True:
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
            if timeout_seconds < (time.perf_counter() - wait_start_time):
                break
        return False

    def wait_for_app_logs_regex(self, container_name, log_entry, timeout_seconds, count=1):
        return self.__wait_for_app_logs_impl(container_name, log_entry, timeout_seconds, count, True)

    def wait_for_app_logs(self, container_name, log_entry, timeout_seconds, count=1):
        return self.__wait_for_app_logs_impl(container_name, log_entry, timeout_seconds, count, False)

    def wait_for_startup_log(self, container_name, timeout_seconds):
        return self.wait_for_app_logs_regex(container_name, self.container_store.get_startup_finished_log_entry(container_name), timeout_seconds, 1)

    def log_app_output(self):
        for container_name in self.container_store.get_container_names():
            _, logs = self.get_app_log(container_name)
            if logs is not None:
                logging.info("Logs of container '%s':", container_name)
                for line in logs.decode("utf-8").splitlines():
                    logging.info(line)

    def check_http_proxy_access(self, container_name, url):
        container_name = self.container_store.get_container_name_with_postfix(container_name)
        (code, output) = self.container_communicator.execute_command(container_name, ["cat", "/var/log/squid/access.log"])
        return code == 0 and url.lower() in output.lower() \
            and ((output.count("TCP_DENIED") != 0
                 and output.count("TCP_MISS") >= output.count("TCP_DENIED"))
                 or output.count("TCP_DENIED") == 0 and "TCP_MISS" in output)

    def check_azure_storage_server_data(self, container_name, test_data):
        container_name = self.container_store.get_container_name_with_postfix(container_name)
        return self.azure_checker.check_azure_storage_server_data(container_name, test_data)

    def add_test_blob(self, blob_name, content="", with_snapshot=False):
        return self.azure_checker.add_test_blob(blob_name, content, with_snapshot)

    def check_azure_blob_and_snapshot_count(self, blob_and_snapshot_count, timeout_seconds):
        return self.azure_checker.check_azure_blob_and_snapshot_count(blob_and_snapshot_count, timeout_seconds)

    def check_azure_blob_storage_is_empty(self, timeout_seconds):
        return self.azure_checker.check_azure_blob_storage_is_empty(timeout_seconds)

    def check_query_results(self, postgresql_container_name, query, number_of_rows, timeout_seconds):
        postgresql_container_name = self.container_store.get_container_name_with_postfix(postgresql_container_name)
        return self.postgres_checker.check_query_results(postgresql_container_name, query, number_of_rows, timeout_seconds)

    def segfault_happened(self):
        return self.segfault

    def check_minifi_log_matches_regex(self, regex, timeout_seconds=60, count=1):
        for container_name in self.container_store.get_container_names("minifi-cpp"):
            line_found = self.wait_for_app_logs_regex(container_name, regex, timeout_seconds, count)
            if line_found:
                return True
        return False

    def check_container_log_contents(self, container_engine, line, timeout_seconds=60, count=1):
        for container_name in self.container_store.get_container_names(container_engine):
            line_found = self.wait_for_app_logs(container_name, line, timeout_seconds, count)
            if line_found:
                return True
        return False

    def check_minifi_log_does_not_contain(self, line, wait_time_seconds):
        time.sleep(wait_time_seconds)
        for container_name in self.container_store.get_container_names("minifi-cpp"):
            _, logs = self.get_app_log(container_name)
            if logs is not None and 1 <= logs.decode("utf-8").count(line):
                return False
        return True

    def wait_for_container_startup_to_finish(self, container_name):
        container_name = self.container_store.get_container_name_with_postfix(container_name)
        startup_success = self.wait_for_startup_log(container_name, 160)
        if not startup_success:
            logging.error("Cluster startup failed for %s", container_name)
            return False
        if not self.container_store.run_post_startup_commands(container_name):
            logging.error("Failed to run post startup commands for container %s", container_name)
            return False
        return True

    def wait_for_all_containers_to_finish_startup(self):
        for container_name in self.container_store.get_container_names():
            if not self.wait_for_container_startup_to_finish(container_name):
                return False
        return True

    def wait_for_peak_memory_usage_to_exceed(self, minimum_peak_memory_usage: int, timeout_seconds: int) -> bool:
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            current_peak_memory_usage = get_peak_memory_usage(get_minifi_pid())
            if current_peak_memory_usage is None:
                logging.warning("Failed to determine peak memory usage")
                return False
            if current_peak_memory_usage > minimum_peak_memory_usage:
                return True
            time.sleep(1)
        logging.warning(f"Peak memory usage ({current_peak_memory_usage}) didnt exceed minimum asserted peak memory usage {minimum_peak_memory_usage}")
        return False

    def wait_for_memory_usage_to_drop_below(self, max_memory_usage: int, timeout_seconds: int) -> bool:
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            current_memory_usage = get_memory_usage(get_minifi_pid())
            if current_memory_usage is None:
                logging.warning("Failed to determine memory usage")
                return False
            if current_memory_usage < max_memory_usage:
                return True
            current_memory_usage = get_memory_usage(get_minifi_pid())
            time.sleep(1)
        logging.warning(f"Memory usage ({current_memory_usage}) is more than the maximum asserted memory usage ({max_memory_usage})")
        return False

    def set_value_on_plc_with_modbus(self, container_name, modbus_cmd):
        return self.modbus_checker.set_value_on_plc_with_modbus(container_name, modbus_cmd)

    def enable_ssl_in_nifi(self):
        self.container_store.enable_ssl_in_nifi()
