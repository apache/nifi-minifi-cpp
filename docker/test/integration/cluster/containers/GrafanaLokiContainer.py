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
import tempfile
import docker.types
import OpenSSL.crypto

from .Container import Container
from ssl_utils.SSL_cert_utils import make_server_cert


class GrafanaLokiOptions:
    def __init__(self):
        self.enable_ssl = False
        self.enable_multi_tenancy = False


class GrafanaLokiContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, options: GrafanaLokiOptions, command=None):
        super().__init__(feature_context, name, "grafana-loki-server", vols, network, image_store, command)
        self.ssl = options.enable_ssl
        extra_ssl_settings = ""
        if self.ssl:
            grafana_loki_cert, grafana_loki_key = make_server_cert(f"grafana-loki-server-{feature_context.id}", feature_context.root_ca_cert, feature_context.root_ca_key)

            self.root_ca_file = tempfile.NamedTemporaryFile(delete=False)
            self.root_ca_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=feature_context.root_ca_cert))
            self.root_ca_file.close()
            os.chmod(self.root_ca_file.name, 0o644)

            self.grafana_loki_cert_file = tempfile.NamedTemporaryFile(delete=False)
            self.grafana_loki_cert_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=grafana_loki_cert))
            self.grafana_loki_cert_file.close()
            os.chmod(self.grafana_loki_cert_file.name, 0o644)

            self.grafana_loki_key_file = tempfile.NamedTemporaryFile(delete=False)
            self.grafana_loki_key_file.write(OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM, pkey=grafana_loki_key))
            self.grafana_loki_key_file.close()
            os.chmod(self.grafana_loki_key_file.name, 0o644)

            extra_ssl_settings = """
  http_tls_config:
    cert_file: /etc/loki/cert.pem
    key_file: /etc/loki/key.pem
    client_ca_file: /etc/loki/root_ca.crt
    client_auth_type: VerifyClientCertIfGiven
"""

        grafana_loki_yml_content = """
auth_enabled: {enable_multi_tenancy}

server:
  http_listen_port: 3100
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
    - from: 2020-10-24
      store: boltdb-shipper
      object_store: filesystem
      schema: v11
      index:
        prefix: index_
        period: 24h

ruler:
  alertmanager_url: http://localhost:9093

analytics:
  reporting_enabled: false
""".format(extra_ssl_settings=extra_ssl_settings, enable_multi_tenancy=options.enable_multi_tenancy)

        self.yaml_file = tempfile.NamedTemporaryFile(delete=False)
        self.yaml_file.write(grafana_loki_yml_content.encode())
        self.yaml_file.close()
        os.chmod(self.yaml_file.name, 0o644)

    def get_startup_finished_log_entry(self):
        return "Loki started"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running Grafana Loki docker container...')

        mounts = [docker.types.Mount(
            type='bind',
            source=self.yaml_file.name,
            target='/etc/loki/local-config.yaml'
        )]

        if self.ssl:
            mounts.append(docker.types.Mount(
                type='bind',
                source=self.root_ca_file.name,
                target='/etc/loki/root_ca.crt'
            ))
            mounts.append(docker.types.Mount(
                type='bind',
                source=self.grafana_loki_cert_file.name,
                target='/etc/loki/cert.pem'
            ))
            mounts.append(docker.types.Mount(
                type='bind',
                source=self.grafana_loki_key_file.name,
                target='/etc/loki/key.pem'
            ))

        self.client.containers.run(
            image="grafana/loki:2.9.2",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'3100/tcp': 3100},
            mounts=mounts,
            entrypoint=self.command)
