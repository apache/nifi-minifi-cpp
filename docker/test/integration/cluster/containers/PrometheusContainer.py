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


class PrometheusContainer(Container):
    def __init__(self, context, name, vols, network, image_store, command=None):
        super().__init__(context, name, 'prometheus', vols, network, image_store, command)
        prometheus_yml_content = """
global:
  scrape_interval: 2s
  evaluation_interval: 15s
scrape_configs:
  - job_name: "minifi"
    static_configs:
      - targets: ["minifi-cpp-flow-{feature_id}:9936"]
""".format(feature_id=self.context.feature_id)
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

        self.client.containers.run(
            image="prom/prometheus:v2.35.0",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'9090/tcp': 9090},
            mounts=[docker.types.Mount(
                type='bind',
                source=self.yaml_file.name,
                target='/etc/prometheus/prometheus.yml'
            )],
            entrypoint=self.command)
