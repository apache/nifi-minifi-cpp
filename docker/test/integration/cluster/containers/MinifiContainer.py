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


import os
import logging
import shortuuid
import shutil
import copy

from .FlowContainer import FlowContainer
from minifi.flow_serialization.Minifi_flow_yaml_serializer import Minifi_flow_yaml_serializer
from minifi.flow_serialization.Minifi_flow_json_serializer import Minifi_flow_json_serializer


class MinifiOptions:
    def __init__(self):
        self.enable_c2 = False
        self.enable_c2_with_ssl = False
        self.enable_provenance = False
        self.enable_prometheus = False
        self.enable_prometheus_with_ssl = False
        self.enable_sql = False
        self.config_format = "json"
        self.use_flow_config_from_url = False
        self.set_ssl_context_properties = False
        self.enable_controller_socket = False
        self.enable_log_metrics_publisher = False


class MinifiContainer(FlowContainer):
    MINIFI_TAG_PREFIX = os.environ['MINIFI_TAG_PREFIX']
    MINIFI_VERSION = os.environ['MINIFI_VERSION']
    MINIFI_ROOT = '/opt/minifi/nifi-minifi-cpp-' + MINIFI_VERSION

    def __init__(self, feature_context, config_dir, options, name, vols, network, image_store, command=None):
        self.options = options

        super().__init__(feature_context=feature_context,
                         config_dir=config_dir,
                         name=name,
                         engine='minifi-cpp',
                         vols=copy.copy(vols),
                         network=network,
                         image_store=image_store,
                         command=command)
        self.container_specific_config_dir = self._create_container_config_dir(self.config_dir)
        os.chmod(self.container_specific_config_dir, 0o777)

    def _create_container_config_dir(self, config_dir):
        container_config_dir = os.path.join(config_dir, str(shortuuid.uuid()))
        os.makedirs(container_config_dir)
        for file_name in os.listdir(config_dir):
            source = os.path.join(config_dir, file_name)
            destination = os.path.join(container_config_dir, file_name)
            if os.path.isfile(source):
                shutil.copy(source, destination)
        return container_config_dir

    def get_startup_finished_log_entry(self):
        return "Starting Flow Controller"

    def _create_config(self):
        if self.options.config_format == "yaml":
            serializer = Minifi_flow_yaml_serializer()
        elif self.options.config_format == "json":
            serializer = Minifi_flow_json_serializer()
        else:
            assert False, "Invalid flow configuration format: {}".format(self.options.config_format)
        test_flow_yaml = serializer.serialize(self.start_nodes, self.controllers)
        logging.info('Using generated flow config yml:\n%s', test_flow_yaml)
        absolute_flow_config_path = os.path.join(self.container_specific_config_dir, "config.yml")
        with open(absolute_flow_config_path, 'wb') as config_file:
            config_file.write(test_flow_yaml.encode('utf-8'))
        os.chmod(absolute_flow_config_path, 0o777)

    def _create_properties(self):
        properties_file_path = os.path.join(self.container_specific_config_dir, 'minifi.properties')
        with open(properties_file_path, 'a') as f:
            if self.options.enable_c2:
                f.write("nifi.c2.enable=true\n")
                f.write(f"nifi.c2.rest.url=http://minifi-c2-server-{self.feature_context.id}:10090/c2/config/heartbeat\n")
                f.write(f"nifi.c2.rest.url.ack=http://minifi-c2-server-{self.feature_context.id}:10090/c2/config/acknowledge\n")
                f.write(f"nifi.c2.flow.base.url=http://minifi-c2-server-{self.feature_context.id}:10090/c2/config/\n")
                f.write("nifi.c2.root.classes=DeviceInfoNode,AgentInformation,FlowInformation\n")
                f.write("nifi.c2.full.heartbeat=false\n")
                f.write("nifi.c2.agent.class=minifi-test-class\n")
                f.write("nifi.c2.agent.identifier=minifi-test-id\n")
            elif self.options.enable_c2_with_ssl:
                f.write("nifi.c2.enable=true\n")
                f.write(f"nifi.c2.rest.url=https://minifi-c2-server-{self.feature_context.id}:10090/c2/config/heartbeat\n")
                f.write(f"nifi.c2.rest.url.ack=https://minifi-c2-server-{self.feature_context.id}:10090/c2/config/acknowledge\n")
                f.write("nifi.c2.rest.ssl.context.service=SSLContextService\n")
                f.write(f"nifi.c2.flow.base.url=https://minifi-c2-server-{self.feature_context.id}:10090/c2/config/\n")
                f.write("nifi.c2.root.classes=DeviceInfoNode,AgentInformation,FlowInformation\n")
                f.write("nifi.c2.full.heartbeat=false\n")
                f.write("nifi.c2.agent.class=minifi-test-class\n")
                f.write("nifi.c2.agent.identifier=minifi-test-id\n")

            if self.options.set_ssl_context_properties:
                f.write("nifi.remote.input.secure=true\n")
                f.write("nifi.security.client.certificate=/tmp/resources/minifi-cpp-flow.crt\n")
                f.write("nifi.security.client.private.key=/tmp/resources/minifi-cpp-flow.key\n")
                f.write("nifi.security.client.pass.phrase=abcdefgh\n")
                f.write("nifi.security.client.ca.certificate=/tmp/resources/root_ca.crt\n")

            if not self.options.enable_provenance:
                f.write("nifi.provenance.repository.class.name=NoOpRepository\n")

            metrics_publisher_classes = []
            if self.options.enable_prometheus or self.options.enable_prometheus_with_ssl:
                f.write("nifi.metrics.publisher.agent.identifier=Agent1\n")
                f.write("nifi.metrics.publisher.PrometheusMetricsPublisher.port=9936\n")
                f.write("nifi.metrics.publisher.PrometheusMetricsPublisher.metrics=RepositoryMetrics,QueueMetrics,PutFileMetrics,processorMetrics/Get.*,FlowInformation,DeviceInfoNode,AgentStatus\n")
                metrics_publisher_classes.append("PrometheusMetricsPublisher")

            if self.options.enable_prometheus_with_ssl:
                f.write("nifi.metrics.publisher.PrometheusMetricsPublisher.certificate=/tmp/resources/minifi_merged_cert.crt\n")
                f.write("nifi.metrics.publisher.PrometheusMetricsPublisher.ca.certificate=/tmp/resources/root_ca.crt\n")

            if self.options.enable_log_metrics_publisher:
                f.write("nifi.metrics.publisher.LogMetricsPublisher.metrics=RepositoryMetrics\n")
                f.write("nifi.metrics.publisher.LogMetricsPublisher.logging.interval=1s\n")
                metrics_publisher_classes.append("LogMetricsPublisher")

            if metrics_publisher_classes:
                f.write("nifi.metrics.publisher.class=" + ",".join(metrics_publisher_classes) + "\n")

            if self.options.use_flow_config_from_url:
                f.write(f"nifi.c2.flow.url=http://minifi-c2-server-{self.feature_context.id}:10090/c2/config?class=minifi-test-class\n")

            if self.options.enable_controller_socket:
                f.write("controller.socket.enable=true\n")
                f.write("controller.socket.host=localhost\n")
                f.write("controller.socket.port=9998\n")
                f.write("controller.socket.local.any.interface=false\n")

    def _setup_config(self):
        self._create_properties()
        if not self.options.use_flow_config_from_url:
            self._create_config()
            self.vols[os.path.join(self.container_specific_config_dir, 'config.yml')] = {"bind": os.path.join(MinifiContainer.MINIFI_ROOT, 'conf', 'config.yml'), "mode": "rw"}

        self.vols[os.path.join(self.container_specific_config_dir, 'minifi.properties')] = {"bind": os.path.join(MinifiContainer.MINIFI_ROOT, 'conf', 'minifi.properties'), "mode": "rw"}
        self.vols[os.path.join(self.container_specific_config_dir, 'minifi-log.properties')] = {"bind": os.path.join(MinifiContainer.MINIFI_ROOT, 'conf', 'minifi-log.properties'), "mode": "rw"}

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running minifi docker container...')
        self._setup_config()

        if self.options.enable_sql:
            image = self.image_store.get_image('minifi-cpp-sql')
        else:
            image = 'apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION

        if self.options.use_flow_config_from_url:
            self.command = ["/bin/sh", "-c", "rm " + MinifiContainer.MINIFI_ROOT + "/conf/config.yml && ./bin/minifi.sh run"]

        self.client.containers.run(
            image,
            detach=True,
            name=self.name,
            network=self.network.name,
            entrypoint=self.command,
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
