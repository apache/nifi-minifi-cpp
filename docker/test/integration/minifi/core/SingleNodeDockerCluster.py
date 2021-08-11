import docker
import logging
import uuid

from .Cluster import Cluster
from .MinifiContainer import MinifiContainer
from .NifiContainer import NifiContainer
from .ZookeeperContainer import ZookeeperContainer
from .KafkaBrokerContainer import KafkaBrokerContainer
from .S3ServerContainer import S3ServerContainer
from .AzureStorageServerContainer import AzureStorageServerContainer
from .HttpProxyContainer import HttpProxyContainer
from .PostgreSQLServerContainer import PostgreSQLServerContainer


class SingleNodeDockerCluster(Cluster):
    """
    A "cluster" which consists of a single docker node. Useful for
    testing or use-cases which do not span multiple compute nodes.
    """

    def __init__(self):
        self.vols = {}
        self.network = self.create_docker_network()
        self.containers = {}

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

    def set_directory_bindings(self, bindings):
        self.vols = bindings
        for container in self.containers.values():
            container.vols = self.vols

    @staticmethod
    def create_docker_network():
        net_name = 'minifi_integration_test_network-' + str(uuid.uuid4())
        logging.debug('Creating network: %s', net_name)
        return docker.from_env().networks.create(net_name)

    def acquire_container(self, name, engine='minifi-cpp'):
        if name is not None and name in self.containers:
            return self.containers[name]

        if name is None and (engine == 'nifi' or engine == 'minifi-cpp'):
            name = engine + '-' + str(uuid.uuid4())
            logging.info('Container name was not provided; using generated name \'%s\'', self.name)

        if engine == 'nifi':
            return self.containers.setdefault(name, NifiContainer(name, self.vols, self.network))
        elif engine == 'minifi-cpp':
            return self.containers.setdefault(name, MinifiContainer(name, self.vols, self.network))
        elif engine == 'kafka-broker':
            self.containers.setdefault('zookeeper', ZookeeperContainer('zookeeper', self.vols, self.network))
            return self.containers.setdefault(name, KafkaBrokerContainer(name, self.vols, self.network))
        elif engine == 'http-proxy':
            return self.containers.setdefault(name, HttpProxyContainer(name, self.vols, self.network))
        elif engine == 's3-server':
            return self.containers.setdefault(name, S3ServerContainer(name, self.vols, self.network))
        elif engine == 'azure-storage-server':
            return self.containers.setdefault(name, AzureStorageServerContainer(name, self.vols, self.network))
        elif engine == 'postgresql-server':
            return self.containers.setdefault(name, PostgreSQLServerContainer(name, self.vols, self.network))
        else:
            raise Exception('invalid flow engine: \'%s\'' % self.engine)

    def deploy(self, name):
        if name is None or name not in self.containers:
            raise Exception('Invalid container to deploy: \'%s\'' % name)

        self.containers[name].deploy()

    def deploy_flow(self):
        for container in self.containers.values():
            container.deploy()
