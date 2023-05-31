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
import uuid
import logging
from .containers.MinifiContainer import MinifiOptions
from .containers.MinifiContainer import MinifiContainer
from .containers.NifiContainer import NifiContainer
from .containers.ZookeeperContainer import ZookeeperContainer
from .containers.KafkaBrokerContainer import KafkaBrokerContainer
from .containers.S3ServerContainer import S3ServerContainer
from .containers.AzureStorageServerContainer import AzureStorageServerContainer
from .containers.FakeGcsServerContainer import FakeGcsServerContainer
from .containers.HttpProxyContainer import HttpProxyContainer
from .containers.PostgreSQLServerContainer import PostgreSQLServerContainer
from .containers.MqttBrokerContainer import MqttBrokerContainer
from .containers.OPCUAServerContainer import OPCUAServerContainer
from .containers.SplunkContainer import SplunkContainer
from .containers.ElasticsearchContainer import ElasticsearchContainer
from .containers.OpensearchContainer import OpensearchContainer
from .containers.SyslogUdpClientContainer import SyslogUdpClientContainer
from .containers.SyslogTcpClientContainer import SyslogTcpClientContainer
from .containers.MinifiAsPodInKubernetesCluster import MinifiAsPodInKubernetesCluster
from .containers.TcpClientContainer import TcpClientContainer
from .containers.PrometheusContainer import PrometheusContainer
from .containers.MinifiC2ServerContainer import MinifiC2ServerContainer


class ContainerStore:
    def __init__(self, network, image_store, kubernetes_proxy):
        self.minifi_options = MinifiOptions()
        self.containers = {}
        self.data_directories = {}
        self.network = network
        self.image_store = image_store
        self.kubernetes_proxy = kubernetes_proxy

    def cleanup(self):
        for container in self.containers.values():
            container.cleanup()
        self.containers = {}
        if self.network:
            logging.info('Cleaning up network: %s', self.network.name)
            self.network.remove()
            self.network = None

    def set_directory_bindings(self, volumes, data_directories):
        self.vols = volumes
        self.data_directories = data_directories
        for container in self.containers.values():
            container.vols = self.vols

    def acquire_container(self, name, engine='minifi-cpp', command=None):
        if name is not None and name in self.containers:
            return self.containers[name]

        if name is None and (engine == 'nifi' or engine == 'minifi-cpp'):
            name = engine + '-' + str(uuid.uuid4())
            logging.info('Container name was not provided; using generated name \'%s\'', name)

        if engine == 'nifi':
            return self.containers.setdefault(name, NifiContainer(self.data_directories["nifi_config_dir"], name, self.vols, self.network, self.image_store, command))
        elif engine == 'minifi-cpp':
            return self.containers.setdefault(name, MinifiContainer(self.data_directories["minifi_config_dir"], self.minifi_options, name, self.vols, self.network, self.image_store, command))
        elif engine == 'kubernetes':
            return self.containers.setdefault(name, MinifiAsPodInKubernetesCluster(self.kubernetes_proxy, self.data_directories["kubernetes_config_dir"], self.minifi_options, name, self.vols, self.network, self.image_store, command))
        elif engine == 'kafka-broker':
            if 'zookeeper' not in self.containers:
                self.containers.setdefault('zookeeper', ZookeeperContainer('zookeeper', self.vols, self.network, self.image_store, command))
            return self.containers.setdefault(name, KafkaBrokerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'http-proxy':
            return self.containers.setdefault(name, HttpProxyContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 's3-server':
            return self.containers.setdefault(name, S3ServerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'azure-storage-server':
            return self.containers.setdefault(name, AzureStorageServerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'fake-gcs-server':
            return self.containers.setdefault(name, FakeGcsServerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'postgresql-server':
            return self.containers.setdefault(name, PostgreSQLServerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'mqtt-broker':
            return self.containers.setdefault(name, MqttBrokerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'opcua-server':
            return self.containers.setdefault(name, OPCUAServerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'splunk':
            return self.containers.setdefault(name, SplunkContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'elasticsearch':
            return self.containers.setdefault(name, ElasticsearchContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == 'opensearch':
            return self.containers.setdefault(name, OpensearchContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == "syslog-udp-client":
            return self.containers.setdefault(name, SyslogUdpClientContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == "syslog-tcp-client":
            return self.containers.setdefault(name, SyslogTcpClientContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == "tcp-client":
            return self.containers.setdefault(name, TcpClientContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == "prometheus":
            return self.containers.setdefault(name, PrometheusContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == "minifi-c2-server":
            return self.containers.setdefault(name, MinifiC2ServerContainer(name, self.vols, self.network, self.image_store, command))
        elif engine == "minifi-c2-server-ssl":
            return self.containers.setdefault(name, MinifiC2ServerContainer(name, self.vols, self.network, self.image_store, command, ssl=True))
        else:
            raise Exception('invalid flow engine: \'%s\'' % engine)

    def deploy_container(self, name):
        if name is None or name not in self.containers:
            raise Exception('Invalid container to deploy: \'%s\'' % name)

        self.containers[name].deploy()

    def deploy_all(self):
        for container in self.containers.values():
            container.deploy()

    def stop_container(self, container_name):
        if container_name not in self.containers:
            logging.error('Could not stop container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].stop()

    def kill_container(self, container_name):
        if container_name not in self.containers:
            logging.error('Could not kill container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].kill()

    def restart_container(self, container_name):
        if container_name not in self.containers:
            logging.error('Could not restart container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].restart()

    def enable_provenance_repository_in_minifi(self):
        self.minifi_options.enable_provenance = True

    def enable_c2_in_minifi(self):
        self.minifi_options.enable_c2 = True

    def enable_c2_with_ssl_in_minifi(self):
        self.minifi_options.enable_c2_with_ssl = True

    def fetch_flow_config_from_c2_url_in_minifi(self):
        self.minifi_options.use_flow_config_from_url = True

    def set_ssl_context_properties_in_minifi(self):
        self.minifi_options.set_ssl_context_properties = True

    def enable_prometheus_in_minifi(self):
        self.minifi_options.enable_prometheus = True

    def enable_sql_in_minifi(self):
        self.minifi_options.enable_sql = True

    def set_yaml_in_minifi(self):
        self.minifi_options.config_format = "yaml"

    def set_controller_socket_properties_in_minifi(self):
        self.minifi_options.enable_controller_socket = True

    def get_startup_finished_log_entry(self, container_name):
        return self.containers[container_name].get_startup_finished_log_entry()

    def log_source(self, container_name):
        return self.containers[container_name].log_source()

    def get_app_log(self, container_name):
        return self.containers[container_name].get_app_log()

    def get_container_names(self, engine=None):
        return [key for key in self.containers.keys() if not engine or self.containers[key].get_engine() == engine]
