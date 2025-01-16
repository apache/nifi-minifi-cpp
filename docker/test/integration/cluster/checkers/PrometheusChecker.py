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
import requests
from prometheus_api_client import PrometheusConnect
from utils import wait_for


class PrometheusChecker:
    def __init__(self):
        self.prometheus_client = PrometheusConnect(url="http://localhost:9090", disable_ssl=True)

    def wait_for_metric_class_on_prometheus(self, metric_class, timeout_seconds):
        return wait_for(lambda: self.verify_metric_class(metric_class), timeout_seconds)

    def wait_for_processor_metric_on_prometheus(self, metric_class, timeout_seconds, processor_name):
        return wait_for(lambda: self.verify_processor_metric(metric_class, processor_name), timeout_seconds)

    def verify_processor_metric(self, metric_class, processor_name):
        if metric_class == "GetFileMetrics":
            return self.verify_getfile_metrics(metric_class, processor_name)
        else:
            return self.verify_general_processor_metrics(metric_class, processor_name)

    def verify_metric_class(self, metric_class):
        if metric_class == "RepositoryMetrics":
            return self.verify_repository_metrics()
        elif metric_class == "QueueMetrics":
            return self.verify_queue_metrics()
        elif metric_class == "FlowInformation":
            return self.verify_flow_information_metrics()
        elif metric_class == "DeviceInfoNode":
            return self.verify_device_info_node_metrics()
        elif metric_class == "AgentStatus":
            return self.verify_agent_status_metrics()
        else:
            raise Exception("Metric class '%s' verification is not implemented" % metric_class)

    def verify_repository_metrics(self):
        label_list = [{'repository_name': 'provenance'}, {'repository_name': 'flowfile'}, {'repository_name': 'content'}]
        # Only flowfile and content repositories are using rocksdb by default, so rocksdb specific metrics are only present there
        return all((self.verify_metrics_exist(['minifi_is_running', 'minifi_is_full', 'minifi_repository_size_bytes', 'minifi_max_repository_size_bytes', 'minifi_repository_entry_count'], 'RepositoryMetrics', labels) for labels in label_list)) and \
            all((self.verify_metric_larger_than_zero('minifi_repository_size_bytes', 'RepositoryMetrics', labels) for labels in label_list[1:3])) and \
            all((self.verify_metrics_exist(['minifi_rocksdb_table_readers_size_bytes', 'minifi_rocksdb_all_memory_tables_size_bytes'], 'RepositoryMetrics', labels) for labels in label_list[1:3]))

    def verify_queue_metrics(self):
        return self.verify_metrics_exist(['minifi_queue_data_size', 'minifi_queue_data_size_max', 'minifi_queue_size', 'minifi_queue_size_max'], 'QueueMetrics')

    def verify_general_processor_metrics(self, metric_class, processor_name):
        labels = {'processor_name': processor_name}
        return self.verify_metrics_exist(['minifi_average_onTrigger_runtime_milliseconds', 'minifi_last_onTrigger_runtime_milliseconds',
                                          'minifi_average_session_commit_runtime_milliseconds', 'minifi_last_session_commit_runtime_milliseconds',
                                          'minifi_incoming_flow_files', 'minifi_incoming_bytes', 'minifi_bytes_read', 'minifi_bytes_written',
                                          'minifi_processing_nanos'], metric_class, labels) and \
            self.verify_metrics_larger_than_zero(['minifi_onTrigger_invocations', 'minifi_transferred_flow_files', 'minifi_transferred_to_success',
                                                  'minifi_transferred_bytes', 'minifi_processing_nanos'],
                                                 metric_class, labels)

    def verify_getfile_metrics(self, metric_class, processor_name):
        labels = {'processor_name': processor_name}
        return self.verify_general_processor_metrics(metric_class, processor_name) and \
            self.verify_metrics_exist(['minifi_input_bytes', 'minifi_accepted_files'], metric_class, labels)

    def verify_flow_information_metrics(self):
        return self.verify_metrics_exist(['minifi_queue_data_size', 'minifi_queue_data_size_max', 'minifi_queue_size', 'minifi_queue_size_max',
                                          'minifi_bytes_read', 'minifi_bytes_written', 'minifi_flow_files_in', 'minifi_flow_files_out', 'minifi_bytes_in', 'minifi_bytes_out',
                                          'minifi_invocations', 'minifi_processing_nanos'], 'FlowInformation') and \
            self.verify_metric_exists('minifi_is_running', 'FlowInformation', {'component_name': 'FlowController'})

    def verify_device_info_node_metrics(self):
        return self.verify_metrics_exist(['minifi_physical_mem', 'minifi_memory_usage', 'minifi_cpu_utilization', 'minifi_cpu_load_average'], 'DeviceInfoNode')

    def verify_agent_status_metrics(self):
        label_list = [{'repository_name': 'flowfile'}, {'repository_name': 'content'}]
        # Only flowfile and content repositories are using rocksdb by default, so rocksdb specific metrics are only present there
        for labels in label_list:
            if not (self.verify_metric_exists('minifi_is_running', 'AgentStatus', labels)
                    and self.verify_metric_exists('minifi_is_full', 'AgentStatus', labels)
                    and self.verify_metric_exists('minifi_max_repository_size_bytes', 'AgentStatus', labels)
                    and self.verify_metric_larger_than_zero('minifi_repository_size_bytes', 'AgentStatus', labels)
                    and self.verify_metric_exists('minifi_repository_entry_count', 'AgentStatus', labels)
                    and self.verify_metric_exists('minifi_rocksdb_table_readers_size_bytes', 'AgentStatus', labels)
                    and self.verify_metric_exists('minifi_rocksdb_all_memory_tables_size_bytes', 'AgentStatus', labels)):
                return False

        # provenance repository is NoOpRepository by default which has zero size
        if not (self.verify_metric_exists('minifi_is_running', 'AgentStatus', {'repository_name': 'provenance'})
                and self.verify_metric_exists('minifi_is_full', 'AgentStatus', {'repository_name': 'provenance'})
                and self.verify_metric_exists('minifi_max_repository_size_bytes', 'AgentStatus', {'repository_name': 'provenance'})
                and self.verify_metric_exists('minifi_repository_size_bytes', 'AgentStatus', {'repository_name': 'provenance'})
                and self.verify_metric_exists('minifi_repository_entry_count', 'AgentStatus', {'repository_name': 'provenance'})):
            return False
        return self.verify_metric_exists('minifi_uptime_milliseconds', 'AgentStatus') and \
            self.verify_metric_exists('minifi_agent_memory_usage_bytes', 'AgentStatus') and \
            self.verify_metric_exists('minifi_agent_cpu_utilization', 'AgentStatus')

    def verify_metric_exists(self, metric_name, metric_class, labels={}):
        labels['metric_class'] = metric_class
        labels['agent_identifier'] = "Agent1"
        return len(self.prometheus_client.get_current_metric_value(metric_name=metric_name, label_config=labels)) > 0

    def verify_metrics_exist(self, metric_names, metric_class, labels={}):
        return all((self.verify_metric_exists(metric_name, metric_class, labels) for metric_name in metric_names))

    def verify_metric_larger_than_zero(self, metric_name, metric_class, labels={}):
        labels['metric_class'] = metric_class
        result = self.prometheus_client.get_current_metric_value(metric_name=metric_name, label_config=labels)
        return len(result) > 0 and int(result[0]['value'][1]) > 0

    def verify_metrics_larger_than_zero(self, metric_names, metric_class, labels={}):
        return all((self.verify_metric_larger_than_zero(metric_name, metric_class, labels) for metric_name in metric_names))

    def verify_all_metric_types_are_defined_once(self):
        response = requests.get("http://127.0.0.1:9936/metrics")
        if response.status_code < 200 or response.status_code >= 300:
            return False

        metric_types = set()
        for line in response.text.split("\n"):
            if line.startswith("# TYPE"):
                metric_type = line.split(" ")[2]
                if metric_type in metric_types:
                    return False
                metric_types.add(metric_type)

        return True
