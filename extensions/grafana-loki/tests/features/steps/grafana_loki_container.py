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

from minifi_test_framework.core.helpers import wait_for_condition, retry_check
from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.file import File
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.ssl_utils import make_server_cert
from docker.errors import ContainerError


class GrafanaLokiOptions:
    def __init__(self, enable_ssl: bool = False, enable_multi_tenancy: bool = False):
        self.enable_ssl = enable_ssl
        self.enable_multi_tenancy = enable_multi_tenancy


class GrafanaLokiContainer(Container):
    def __init__(self, test_context: MinifiTestContext, options: GrafanaLokiOptions):
        super().__init__("grafana/loki:3.2.1", f"grafana-loki-server-{test_context.scenario_id}", test_context.network)
        extra_ssl_settings = ""
        if options.enable_ssl:
            grafana_loki_cert, grafana_loki_key = make_server_cert(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)

            root_ca_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)
            self.files.append(File("/etc/loki/root_ca.crt", root_ca_content, permissions=0o644))

            grafana_loki_cert_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=grafana_loki_cert)
            self.files.append(File("/etc/loki/cert.pem", grafana_loki_cert_content, permissions=0o644))

            grafana_loki_key_content = crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=grafana_loki_key)
            self.files.append(File("/etc/loki/key.pem", grafana_loki_key_content, permissions=0o644))

            extra_ssl_settings = """
  http_tls_config:
    cert_file: /etc/loki/cert.pem
    key_file: /etc/loki/key.pem
    client_ca_file: /etc/loki/root_ca.crt
    client_auth_type: VerifyClientCertIfGiven

  grpc_tls_config:
    cert_file: /etc/loki/cert.pem
    key_file: /etc/loki/key.pem
    client_ca_file: /etc/loki/root_ca.crt
    client_auth_type: VerifyClientCertIfGiven

query_scheduler:
  grpc_client_config:
    grpc_compression: snappy
    tls_enabled: true
    tls_ca_path: /etc/loki/root_ca.crt
    tls_insecure_skip_verify: true

ingester_client:
  grpc_client_config:
    grpc_compression: snappy
    tls_enabled: true
    tls_ca_path: /etc/loki/root_ca.crt
    tls_insecure_skip_verify: true

frontend:
  grpc_client_config:
    grpc_compression: snappy
    tls_enabled: true
    tls_ca_path: /etc/loki/root_ca.crt
    tls_insecure_skip_verify: true

frontend_worker:
  grpc_client_config:
    grpc_compression: snappy
    tls_enabled: true
    tls_ca_path: /etc/loki/root_ca.crt
    tls_insecure_skip_verify: true
"""

        grafana_loki_yml_content = """
auth_enabled: {enable_multi_tenancy}

server:
  http_listen_port: 3100
  grpc_listen_port: 9095
{extra_ssl_settings}

common:
  path_prefix: /loki
  storage:
    filesystem:
      chunks_directory: /loki/chunks
      rules_directory: /loki/rules
  replication_factor: 1
  ring:
    kvstore:
      store: inmemory

schema_config:
  configs:
    - from: 2020-05-15
      store: tsdb
      object_store: filesystem
      schema: v13
      index:
        prefix: index_
        period: 24h

ruler:
  alertmanager_url: http://localhost:9093

analytics:
  reporting_enabled: false
""".format(extra_ssl_settings=extra_ssl_settings, enable_multi_tenancy=options.enable_multi_tenancy)

        self.files.append(File("/etc/loki/local-config.yaml", grafana_loki_yml_content.encode(), permissions=0o644))

    def deploy(self):
        super().deploy()
        finished_str = "Loki started"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=120,
            bail_condition=lambda: self.exited,
            context=None)

    @retry_check()
    def are_lines_present(self, lines: str, timeout: int, ssl: bool, tenant_id: str = "") -> bool:
        try:
            self.client.containers.run("minifi-grafana-loki-helper:latest", ["python", "/scripts/check_log_lines_on_grafana.py", self.container_name, lines, str(timeout), str(ssl), tenant_id],
                                       remove=True, stdout=True, stderr=True, network=self.network.name)
            return True
        except ContainerError as e:
            stdout = e.stdout.decode("utf-8", errors="replace") if hasattr(e, "stdout") and e.stdout else ""
            stderr = e.stderr.decode("utf-8", errors="replace") if hasattr(e, "stderr") and e.stderr else ""
            logging.error(f"Failed to run python command in grafana loki helper docker with error: '{e}', stdout: '{stdout}', stderr: '{stderr}'")
            return False
        except Exception as e:
            logging.error(f"Unexpected error while running python command in grafana loki helper docker: '{e}'")
            return False
