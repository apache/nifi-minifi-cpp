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
        self.enable_provenance = False
        self.enable_sql = False
        self.config_format = "json"
        self.set_ssl_context_properties = False
        self.enable_log_metrics_publisher = False
        if "true" in os.environ['MINIFI_FIPS']:
            self.enable_openssl_fips_mode = True
        else:
            self.enable_openssl_fips_mode = False
        self.download_llama_model = False


class MinifiLocations:
    MINIFI_TAG_PREFIX = os.environ['MINIFI_TAG_PREFIX']
    MINIFI_VERSION = os.environ['MINIFI_VERSION']

    def __init__(self):
        if "rpm" in MinifiLocations.MINIFI_TAG_PREFIX:
            self.run_minifi_cmd = '/usr/bin/minifi'
            self.config_path = '/etc/nifi-minifi-cpp/config.yml'
            self.properties_path = '/etc/nifi-minifi-cpp/minifi.properties'
            self.log_properties_path = '/etc/nifi-minifi-cpp/minifi-log.properties'
            self.uid_properties_path = '/etc/nifi-minifi-cpp/minifi-uid.properties'
            self.models_path = '/var/lib/nifi-minifi-cpp/models'
            self.minifi_home = '/var/lib/nifi-minifi-cpp'
        else:
            self.run_minifi_cmd = '/opt/minifi/minifi-current/bin/minifi.sh run'
            self.config_path = '/opt/minifi/minifi-current/conf/config.yml'
            self.properties_path = '/opt/minifi/minifi-current/conf/minifi.properties'
            self.log_properties_path = '/opt/minifi/minifi-current/conf/minifi-log.properties'
            self.uid_properties_path = '/opt/minifi/minifi-current/conf/minifi-uid.properties'
            self.models_path = '/opt/minifi/minifi-current/models'
            self.minifi_home = '/opt/minifi/minifi-current'


class MinifiContainer(FlowContainer):
    MINIFI_TAG_PREFIX = os.environ['MINIFI_TAG_PREFIX']
    MINIFI_VERSION = os.environ['MINIFI_VERSION']
    MINIFI_LOCATIONS = MinifiLocations()

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
        test_flow_yaml = serializer.serialize(self.start_nodes, self.controllers, self.parameter_context_name, self.parameter_contexts)
        logging.info('Using generated flow config yml:\n%s', test_flow_yaml)
        absolute_flow_config_path = os.path.join(self.container_specific_config_dir, "config.yml")
        with open(absolute_flow_config_path, 'wb') as config_file:
            config_file.write(test_flow_yaml.encode('utf-8'))
        os.chmod(absolute_flow_config_path, 0o777)

    def _create_properties(self):
        properties_file_path = os.path.join(self.container_specific_config_dir, 'minifi.properties')
        with open(properties_file_path, 'a') as f:
            f.write("nifi.flow.configuration.file={conf_path}\n".format(conf_path=MinifiContainer.MINIFI_LOCATIONS.config_path))
            f.write("nifi.provenance.repository.directory.default={minifi_home}/provenance_repository\n".format(minifi_home=MinifiContainer.MINIFI_LOCATIONS.minifi_home))
            f.write("nifi.flowfile.repository.directory.default={minifi_home}/flowfile_repository\n".format(minifi_home=MinifiContainer.MINIFI_LOCATIONS.minifi_home))
            f.write("nifi.database.content.repository.directory.default={minifi_home}/content_repository\n".format(minifi_home=MinifiContainer.MINIFI_LOCATIONS.minifi_home))

            if self.options.set_ssl_context_properties:
                f.write("nifi.remote.input.secure=true\n")
                f.write("nifi.security.client.certificate=/tmp/resources/minifi_client.crt\n")
                f.write("nifi.security.client.private.key=/tmp/resources/minifi_client.key\n")
                f.write("nifi.security.client.ca.certificate=/tmp/resources/root_ca.crt\n")

            if not self.options.enable_provenance:
                f.write("nifi.provenance.repository.class.name=NoOpRepository\n")

            metrics_publisher_classes = []
            if self.options.enable_log_metrics_publisher:
                f.write("nifi.metrics.publisher.LogMetricsPublisher.metrics=RepositoryMetrics\n")
                f.write("nifi.metrics.publisher.LogMetricsPublisher.logging.interval=1s\n")
                metrics_publisher_classes.append("LogMetricsPublisher")

            if metrics_publisher_classes:
                f.write("nifi.metrics.publisher.class=" + ",".join(metrics_publisher_classes) + "\n")

            if self.options.enable_openssl_fips_mode:
                f.write("nifi.openssl.fips.support.enable=true\n")
            else:
                f.write("nifi.openssl.fips.support.enable=false\n")

    def _setup_config(self):
        self._create_properties()
        self._create_config()
        self.vols[os.path.join(self.container_specific_config_dir, 'config.yml')] = {"bind": MinifiContainer.MINIFI_LOCATIONS.config_path, "mode": "rw"}
        self.vols[os.path.join(self.container_specific_config_dir, 'minifi.properties')] = {"bind": MinifiContainer.MINIFI_LOCATIONS.properties_path, "mode": "rw"}
        self.vols[os.path.join(self.container_specific_config_dir, 'minifi-log.properties')] = {"bind": MinifiContainer.MINIFI_LOCATIONS.log_properties_path, "mode": "rw"}

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running minifi docker container...')
        self._setup_config()

        if self.options.enable_sql:
            image = self.image_store.get_image('minifi-cpp-sql')
        elif self.options.download_llama_model:
            image = self.image_store.get_image('minifi-cpp-with-llamacpp-model')
        else:
            image = 'apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION

        self.client.containers.run(
            image,
            detach=True,
            name=self.name,
            network=self.network.name,
            entrypoint=self.command,
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
