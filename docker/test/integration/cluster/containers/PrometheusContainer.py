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

from .Container import Container
from OpenSSL import crypto
from ssl_utils.SSL_cert_utils import make_cert_without_extended_usage


class PrometheusContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None, ssl=False):
        engine = "prometheus-ssl" if ssl else "prometheus"
        super().__init__(feature_context, name, engine, vols, network, image_store, command)
        self.ssl = ssl
        extra_ssl_settings = ""
        if ssl:
            prometheus_cert, prometheus_key = make_cert_without_extended_usage(f"prometheus-{feature_context.id}", feature_context.root_ca_cert, feature_context.root_ca_key)

            self.root_ca_file = tempfile.NamedTemporaryFile(delete=False)
            self.root_ca_file.write(crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=feature_context.root_ca_cert))
            self.root_ca_file.close()
            os.chmod(self.root_ca_file.name, 0o644)

            self.prometheus_cert_file = tempfile.NamedTemporaryFile(delete=False)
            self.prometheus_cert_file.write(crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=prometheus_cert))
            self.prometheus_cert_file.close()
            os.chmod(self.prometheus_cert_file.name, 0o644)

            self.prometheus_key_file = tempfile.NamedTemporaryFile(delete=False)
            self.prometheus_key_file.write(crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=prometheus_key))
            self.prometheus_key_file.close()
            os.chmod(self.prometheus_key_file.name, 0o644)

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
      - targets: ["minifi-cpp-flow-{feature_id}:9936"]
{extra_ssl_settings}
""".format(feature_id=self.feature_context.id, extra_ssl_settings=extra_ssl_settings)

        self.yaml_file = tempfile.NamedTemporaryFile(delete=False)
        self.yaml_file.write(prometheus_yml_content.encode())
        self.yaml_file.close()
        os.chmod(self.yaml_file.name, 0o644)

    def get_startup_finished_log_entry(self):
        return "Server is ready to receive web requests."

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running Prometheus docker container...')

        mounts = [docker.types.Mount(
            type='bind',
            source=self.yaml_file.name,
            target='/etc/prometheus/prometheus.yml'
        )]

        if self.ssl:
            mounts.append(docker.types.Mount(
                type='bind',
                source=self.root_ca_file.name,
                target='/etc/prometheus/certs/root-ca.pem'
            ))

        self.client.containers.run(
            image="prom/prometheus:v2.35.0",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'9090/tcp': 9090},
            mounts=mounts,
            entrypoint=self.command)
