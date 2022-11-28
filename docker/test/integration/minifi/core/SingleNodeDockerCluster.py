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


import docker
import logging
import uuid

from .Cluster import Cluster
from .MinifiContainer import MinifiContainer
from .TransientMinifiContainer import TransientMinifiContainer
from .MinifiWithProvenanceRepoContainer import MinifiWithProvenanceRepoContainer
from .NifiContainer import NifiContainer
from .ZookeeperContainer import ZookeeperContainer
from .KafkaBrokerContainer import KafkaBrokerContainer
from .S3ServerContainer import S3ServerContainer
from .AzureStorageServerContainer import AzureStorageServerContainer
from .FakeGcsServerContainer import FakeGcsServerContainer
from .HttpProxyContainer import HttpProxyContainer
from .PostgreSQLServerContainer import PostgreSQLServerContainer
from .MqttBrokerContainer import MqttBrokerContainer
from .OPCUAServerContainer import OPCUAServerContainer
from .SplunkContainer import SplunkContainer
from .ElasticsearchContainer import ElasticsearchContainer
from .OpensearchContainer import OpensearchContainer
from .SyslogUdpClientContainer import SyslogUdpClientContainer
from .SyslogTcpClientContainer import SyslogTcpClientContainer
from .MinifiAsPodInKubernetesCluster import MinifiAsPodInKubernetesCluster
from .TcpClientContainer import TcpClientContainer
from .PrometheusContainer import PrometheusContainer
from .MinifiC2ServerContainer import MinifiC2ServerContainer
from .MinifiWithHttpsC2Config import MinifiWithHttpsC2Config


class SingleNodeDockerCluster(Cluster):
    """
    A "cluster" which consists of a single docker node. Useful for
    testing or use-cases which do not span multiple compute nodes.
    """

    def __init__(self, context):
        self.vols = {}
        self.network = self.create_docker_network()
        self.containers = {}
        self.image_store = context.image_store
        self.data_directories = {}
        self.kubernetes_proxy = context.kubernetes_proxy

        # Get docker client
        self.client = docker.from_env()

    def __del__(self):
        self.cleanup()

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

    @staticmethod
    def create_docker_network():
        net_name = 'minifi_integration_test_network-' + str(uuid.uuid4())
        logging.debug('Creating network: %s', net_name)
        return docker.from_env().networks.create(net_name)

    def acquire_container(self, name, engine='minifi-cpp', command=None):
        if name is not None and name in self.containers:
            return self.containers[name]

        if name is None and (engine == 'nifi' or engine == 'minifi-cpp'):
            name = engine + '-' + str(uuid.uuid4())
            logging.info('Container name was not provided; using generated name \'%s\'', name)

        if engine == 'nifi':
            return self.containers.setdefault(name, NifiContainer(self.data_directories["nifi_config_dir"], name, self.vols, self.network, self.image_store, command))
        elif engine == 'minifi-cpp':
            return self.containers.setdefault(name, MinifiContainer(self.data_directories["minifi_config_dir"], name, self.vols, self.network, self.image_store, command))
        elif engine == 'kubernetes':
            return self.containers.setdefault(name, MinifiAsPodInKubernetesCluster(self.kubernetes_proxy, self.data_directories["minifi_config_dir"], name, self.vols, self.network, self.image_store, command))
        elif engine == 'transient-minifi':
            return self.containers.setdefault(name, TransientMinifiContainer(self.data_directories["minifi_config_dir"], name, self.vols, self.network, self.image_store, command))
        elif engine == 'minifi-cpp-with-provenance-repo':
            return self.containers.setdefault(name, MinifiWithProvenanceRepoContainer(self.data_directories["minifi_config_dir"], name, self.vols, self.network, self.image_store, command))
        elif engine == 'minifi-cpp-with-https-c2-config':
            return self.containers.setdefault(name, MinifiWithHttpsC2Config(self.data_directories["minifi_config_dir"], name, self.vols, self.network, self.image_store, command))
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

    def deploy(self, name):
        if name is None or name not in self.containers:
            raise Exception('Invalid container to deploy: \'%s\'' % name)

        self.containers[name].deploy()

    def deploy_flow(self, container_name=None):
        if container_name is not None:
            if container_name not in self.containers:
                logging.error('Could not start container because it is not found: \'%s\'', container_name)
                return
            self.containers[container_name].deploy()
            return
        for container in self.containers.values():
            container.deploy()

    def stop_flow(self, container_name):
        if container_name not in self.containers:
            logging.error('Could not stop container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].stop()

    def kill_flow(self, container_name):
        if container_name not in self.containers:
            logging.error('Could not kill container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].kill()

    def restart_flow(self, container_name):
        if container_name not in self.containers:
            logging.error('Could not restart container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].restart()
