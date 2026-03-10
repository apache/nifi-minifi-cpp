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
from OpenSSL import crypto
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.ssl_utils import make_cert_without_extended_usage
from minifi_test_framework.containers.file import File


class PrometheusContainer(Container):
    def __init__(self, test_context: MinifiTestContext, ssl: bool = False):
        super().__init__("prom/prometheus:v3.9.1", f"prometheus-{test_context.scenario_id}", test_context.network)
        self.scenario_id = test_context.scenario_id
        extra_ssl_settings = ""
        if ssl:
            prometheus_cert, prometheus_key = make_cert_without_extended_usage(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)

            root_ca_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)
            self.files.append(File("/etc/prometheus/certs/root-ca.pem", root_ca_content, permissions=0o644))

            prometheus_cert_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=prometheus_cert)
            self.files.append(File("/etc/prometheus/certs/prometheus.crt", prometheus_cert_content, permissions=0o644))

            prometheus_key_content = crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=prometheus_key)
            self.files.append(File("/etc/prometheus/certs/prometheus.key", prometheus_key_content, permissions=0o644))

            extra_ssl_settings = """
    scheme: https
    tls_config:
      ca_file: /etc/prometheus/certs/root-ca.pem
"""
        prometheus_yml_content = """
global:
  scrape_interval: 2s
  evaluation_interval: 15s
scrape_configs:
  - job_name: "minifi"
    static_configs:
      - targets: ["minifi-primary-{scenario_id}:9936"]
{extra_ssl_settings}
""".format(scenario_id=test_context.scenario_id, extra_ssl_settings=extra_ssl_settings)

        self.files.append(File("/etc/prometheus/prometheus.yml", prometheus_yml_content, permissions=0o666))

    def deploy(self):
        super().deploy()
        finished_str = "Server is ready to receive web requests."
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=60,
            bail_condition=lambda: self.exited,
            context=None
        )

    def check_metric_class_on_prometheus(self, metric_class: str) -> bool:
        try:
            self.client.containers.run("minifi-prometheus-helper:latest", ["python", "/scripts/prometheus_checker.py", "--prometheus-host", self.container_name, "--metric-class", metric_class], remove=True, network=self.network.name)
            return True
        except Exception:
            logging.error(f"Failed to check metric class {metric_class} on Prometheus")
            return False

    def check_processor_metric_on_prometheus(self, metric_class: str, processor_name: str) -> bool:
        try:
            self.client.containers.run("minifi-prometheus-helper:latest", ["python", "/scripts/prometheus_checker.py", "--prometheus-host", self.container_name, "--metric-class", metric_class, "--processor-name", processor_name], remove=True, network=self.network.name)
            return True
        except Exception:
            logging.error(f"Failed to check processor metric class {metric_class} for processor {processor_name} on Prometheus")
            return False

    def check_all_metric_types_defined_once(self) -> bool:
        try:
            self.client.containers.run("minifi-prometheus-helper:latest", ["python", "/scripts/prometheus_checker.py", "--verify-metrics-defined-once", f"minifi-primary-{self.scenario_id}"], remove=True, network=self.network.name)
            return True
        except Exception:
            logging.error("Failed check that all metric types are defined once on Prometheus")
            return False
