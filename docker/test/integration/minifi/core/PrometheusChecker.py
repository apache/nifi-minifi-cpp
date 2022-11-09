import time
from prometheus_api_client import PrometheusConnect


class PrometheusChecker:
    def __init__(self):
        self.prometheus_client = PrometheusConnect(url="http://localhost:9090", disable_ssl=True)

    def wait_for_metric_class_on_prometheus(self, metric_class, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.verify_metric_class(metric_class):
                return True
            time.sleep(1)
        return False

    def wait_for_processor_metric_on_prometheus(self, metric_class, timeout_seconds, processor_name):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.verify_processor_metric(metric_class, processor_name):
                return True
            time.sleep(1)
        return False

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
        label_list = [{'repository_name': 'provenance'}, {'repository_name': 'flowfile'}]
        return all((self.verify_metrics_exist(['minifi_is_running', 'minifi_is_full', 'minifi_repository_size'], 'RepositoryMetrics', labels) for labels in label_list))

    def verify_queue_metrics(self):
        return self.verify_metrics_exist(['minifi_queue_data_size', 'minifi_queue_data_size_max', 'minifi_queue_size', 'minifi_queue_size_max'], 'QueueMetrics')

    def verify_general_processor_metrics(self, metric_class, processor_name):
        labels = {'processor_name': processor_name}
        return self.verify_metrics_exist(['minifi_average_onTrigger_runtime_milliseconds', 'minifi_last_onTrigger_runtime_milliseconds'], metric_class, labels) and \
            self.verify_metrics_larger_than_zero(['minifi_onTrigger_invocations', 'minifi_transferred_flow_files', 'minifi_transferred_to_success', 'minifi_transferred_bytes'], metric_class, labels)

    def verify_getfile_metrics(self, metric_class, processor_name):
        labels = {'processor_name': processor_name}
        return self.verify_general_processor_metrics(metric_class, processor_name) and \
            self.verify_metrics_exist(['minifi_input_bytes', 'minifi_accepted_files'], metric_class, labels)

    def verify_flow_information_metrics(self):
        return self.verify_metrics_exist(['minifi_queue_data_size', 'minifi_queue_data_size_max', 'minifi_queue_size', 'minifi_queue_size_max'], 'FlowInformation') and \
            self.verify_metric_exists('minifi_is_running', 'FlowInformation', {'component_name': 'FlowController'})

    def verify_device_info_node_metrics(self):
        return self.verify_metrics_exist(['minifi_physical_mem', 'minifi_memory_usage', 'minifi_cpu_utilization'], 'DeviceInfoNode')

    def verify_agent_status_metrics(self):
        label_list = [{'repository_name': 'provenance'}, {'repository_name': 'flowfile'}]
        for labels in label_list:
            if not (self.verify_metric_exists('minifi_is_running', 'AgentStatus', labels)
                    and self.verify_metric_exists('minifi_is_full', 'AgentStatus', labels)
                    and self.verify_metric_exists('minifi_repository_size', 'AgentStatus', labels)):
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
