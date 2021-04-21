import gzip
import docker
import logging
import os
import tarfile
import uuid

from collections import OrderedDict
from io import BytesIO
from textwrap import dedent

from .Cluster import Cluster
from ..flow_serialization.Minifi_flow_yaml_serializer import Minifi_flow_yaml_serializer
from ..flow_serialization.Nifi_flow_xml_serializer import Nifi_flow_xml_serializer


class SingleNodeDockerCluster(Cluster):
    """
    A "cluster" which consists of a single docker node. Useful for
    testing or use-cases which do not span multiple compute nodes.
    """

    def __init__(self):
        self.minifi_version = os.environ['MINIFI_VERSION']
        self.nifi_version = '1.7.0'
        self.engine = 'minifi-cpp'
        self.flow = None
        self.name = None
        self.vols = {}
        self.minifi_root = '/opt/minifi/nifi-minifi-cpp-' + self.minifi_version
        self.nifi_root = '/opt/nifi/nifi-' + self.nifi_version
        self.kafka_broker_root = '/opt/kafka'
        self.network = None
        self.containers = OrderedDict()
        self.images = []
        self.tmp_files = []

        # Get docker client
        self.client = docker.from_env()

    def __del__(self):
        """
        Clean up ephemeral cluster resources
        """

        # Containers and networks are expected to be freed outside of this function

        # Clean up images
        for image in reversed(self.images):
            logging.info('Cleaning up image: %s', image[0].id)
            self.client.images.remove(image[0].id, force=True)

        # Clean up tmp files
        for tmp_file in self.tmp_files:
            os.remove(tmp_file)

    def set_name(self, name):
        self.name = name

    def get_name(self):
        return self.name

    def set_engine(self, engine):
        self.engine = engine

    def get_engine(self):
        return self.engine

    def get_flow(self):
        return self.flow

    def set_flow(self, flow):
        self.flow = flow

    def set_directory_bindings(self, bindings):
        self.vols = bindings

    @staticmethod
    def create_docker_network():
        net_name = 'minifi_integration_test_network-' + str(uuid.uuid4())
        logging.info('Creating network: %s', net_name)
        return docker.from_env().networks.create(net_name)

    def set_network(self, network):
        self.network = network

    def deploy_flow(self):
        """
        Compiles the flow to a valid config file and overlays it into a new image.
        """

        if self.vols is None:
            self.vols = {}

        if self.name is None:
            self.name = self.engine + '-' + str(uuid.uuid4())
            logging.info('Flow name was not provided; using generated name \'%s\'', self.name)

        logging.info('Deploying %s flow \"%s\"...', self.engine, self.name)

        # Create network if necessary
        if self.network is None:
            self.set_network(self.create_docker_network())

        if self.engine == 'nifi':
            self.deploy_nifi_flow()
        elif self.engine == 'minifi-cpp':
            self.deploy_minifi_cpp_flow()
        elif self.engine == 'kafka-broker':
            self.deploy_kafka_broker()
        elif self.engine == 'http-proxy':
            self.deploy_http_proxy()
        elif self.engine == 's3-server':
            self.deploy_s3_server()
        elif self.engine == 'azure-storage-server':
            self.deploy_azure_storage_server()
        else:
            raise Exception('invalid flow engine: \'%s\'' % self.engine)

    def deploy_minifi_cpp_flow(self):

        # Build configured image
        dockerfile = dedent("""FROM {base_image}
                USER root
                ADD config.yml {minifi_root}/conf/config.yml
                RUN chown minificpp:minificpp {minifi_root}/conf/config.yml
                USER minificpp
                """.format(base_image='apacheminificpp:' + self.minifi_version,
                           minifi_root=self.minifi_root))

        serializer = Minifi_flow_yaml_serializer()
        test_flow_yaml = serializer.serialize(self.flow)
        logging.info('Using generated flow config yml:\n%s', test_flow_yaml)

        conf_file_buffer = BytesIO()

        try:
            conf_file_buffer.write(test_flow_yaml.encode('utf-8'))
            conf_file_len = conf_file_buffer.tell()
            conf_file_buffer.seek(0)

            context_files = [
                {
                    'name': 'config.yml',
                    'size': conf_file_len,
                    'file_obj': conf_file_buffer
                }
            ]

            configured_image = self.build_image(dockerfile, context_files)

        finally:
            conf_file_buffer.close()

        container = self.client.containers.run(
            configured_image[0],
            detach=True,
            name=self.name,
            network=self.network.name,
            volumes=self.vols)
        self.network.reload()

        logging.info('Started container \'%s\'', container.name)

        logging.info('Adding container \'%s\'', container.name)
        self.containers[container.name] = container

    def deploy_nifi_flow(self):
        dockerfile = dedent(r"""FROM {base_image}
                USER root
                ADD flow.xml.gz {nifi_root}/conf/flow.xml.gz
                RUN chown nifi:nifi {nifi_root}/conf/flow.xml.gz
                RUN sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties
                RUN sed -i -e 's/^\(nifi.remote.input.socket.port\)=.*/\1=5000/' {nifi_root}/conf/nifi.properties
                USER nifi
                """.format(name=self.name,
                           base_image='apache/nifi:' + self.nifi_version,
                           nifi_root=self.nifi_root))

        serializer = Nifi_flow_xml_serializer()
        test_flow_xml = serializer.serialize(self.flow, self.nifi_version)
        logging.info('Using generated flow config xml:\n%s', test_flow_xml)

        conf_file_buffer = BytesIO()

        try:
            with gzip.GzipFile(mode='wb', fileobj=conf_file_buffer) as conf_gz_file_buffer:
                conf_gz_file_buffer.write(test_flow_xml.encode())
            conf_file_len = conf_file_buffer.tell()
            conf_file_buffer.seek(0)

            context_files = [
                {
                    'name': 'flow.xml.gz',
                    'size': conf_file_len,
                    'file_obj': conf_file_buffer
                }
            ]

            configured_image = self.build_image(dockerfile, context_files)

        finally:
            conf_file_buffer.close()

        logging.info('Creating and running docker container for flow...')

        container = self.client.containers.run(
            configured_image[0],
            detach=True,
            name=self.name,
            hostname=self.name,
            network=self.network.name,
            volumes=self.vols)

        logging.info('Started container \'%s\'', container.name)

        logging.info('Adding container \'%s\'', container.name)
        self.containers[container.name] = container

    def deploy_kafka_broker(self):
        logging.info('Creating and running docker containers for kafka broker...')
        zookeeper = self.client.containers.run(
            self.client.images.pull("wurstmeister/zookeeper:latest"),
            detach=True,
            name='zookeeper',
            network=self.network.name,
            ports={'2181/tcp': 2181})
        logging.info('Adding container \'%s\'', zookeeper.name)
        self.containers[zookeeper.name] = zookeeper

        test_dir = os.environ['PYTHONPATH'].split(':')[-1]  # Based on DockerVerify.sh
        broker_image = self.build_image_by_path(test_dir + "/resources/kafka_broker", 'minifi-kafka')
        broker = self.client.containers.run(
            broker_image[0],
            detach=True,
            name='kafka-broker',
            network=self.network.name,
            ports={'9092/tcp': 9092, '29092/tcp' : 29092},
            environment=[
                "KAFKA_BROKER_ID=1",
                'ALLOW_PLAINTEXT_LISTENER: "yes"',
                "KAFKA_LISTENERS=PLAINTEXT://kafka-broker:9092,SSL://kafka-broker:9093,PLAINTEXT_HOST://0.0.0.0:29092",
                "KAFKA_LISTENER_SECURITY_PROTOCOL_MAP=PLAINTEXT:PLAINTEXT,PLAINTEXT_HOST:PLAINTEXT,SSL:SSL",
                "KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://kafka-broker:9092,SSL://kafka-broker:9093,PLAINTEXT_HOST://localhost:29092",
                "KAFKA_ZOOKEEPER_CONNECT=zookeeper:2181"],
            )
        logging.info('Adding container \'%s\'', broker.name)
        self.containers[broker.name] = broker

        dockerfile = dedent("""FROM {base_image}
                USER root
                CMD $KAFKA_HOME/bin/kafka-console-consumer.sh --bootstrap-server kafka-broker:9092 --topic test > heaven_signal.txt
                """.format(base_image='wurstmeister/kafka:2.13-2.7.0'))
        configured_image = self.build_image(dockerfile, [])
        consumer = self.client.containers.run(
            configured_image[0],
            detach=True,
            name='kafka-consumer',
            network=self.network.name)
        logging.info('Adding container \'%s\'', consumer.name)
        self.containers[consumer.name] = consumer

    def deploy_http_proxy(self):
        logging.info('Creating and running http-proxy docker container...')
        dockerfile = dedent("""FROM {base_image}
                RUN apt update && apt install -y apache2-utils
                RUN htpasswd -b -c /etc/squid/.squid_users {proxy_username} {proxy_password}
                RUN echo 'auth_param basic program /usr/lib/squid3/basic_ncsa_auth /etc/squid/.squid_users'  > /etc/squid/squid.conf && \
                    echo 'auth_param basic realm proxy' >> /etc/squid/squid.conf && \
                    echo 'acl authenticated proxy_auth REQUIRED' >> /etc/squid/squid.conf && \
                    echo 'http_access allow authenticated' >> /etc/squid/squid.conf && \
                    echo 'http_port {proxy_port}' >> /etc/squid/squid.conf
                ENTRYPOINT ["/sbin/entrypoint.sh"]
                """.format(base_image='sameersbn/squid:3.5.27-2', proxy_username='admin', proxy_password='test101', proxy_port='3128'))
        configured_image = self.build_image(dockerfile, [])
        consumer = self.client.containers.run(
            configured_image[0],
            detach=True,
            name='http-proxy',
            network=self.network.name,
            ports={'3128/tcp': 3128})
        self.containers[consumer.name] = consumer

    def deploy_s3_server(self):
        server = self.client.containers.run(
            "adobe/s3mock:2.1.28",
            detach=True,
            name='s3-server',
            network=self.network.name,
            ports={'9090/tcp': 9090, '9191/tcp': 9191},
            environment=["initialBuckets=test_bucket"])
        self.containers[server.name] = server

    def deploy_azure_storage_server(self):
        server = self.client.containers.run(
            "mcr.microsoft.com/azure-storage/azurite",
            detach=True,
            name='azure-storage-server',
            network=self.network.name,
            ports={'10000/tcp': 10000, '10001/tcp': 10001})
        self.containers[server.name] = server

    def build_image(self, dockerfile, context_files):
        conf_dockerfile_buffer = BytesIO()
        docker_context_buffer = BytesIO()

        try:
            # Overlay conf onto base nifi image
            conf_dockerfile_buffer.write(dockerfile.encode())
            conf_dockerfile_buffer.seek(0)

            with tarfile.open(mode='w', fileobj=docker_context_buffer) as docker_context:
                dockerfile_info = tarfile.TarInfo('Dockerfile')
                dockerfile_info.size = conf_dockerfile_buffer.getbuffer().nbytes
                docker_context.addfile(dockerfile_info,
                                       fileobj=conf_dockerfile_buffer)

                for context_file in context_files:
                    file_info = tarfile.TarInfo(context_file['name'])
                    file_info.size = context_file['size']
                    docker_context.addfile(file_info,
                                           fileobj=context_file['file_obj'])
            docker_context_buffer.seek(0)

            logging.info('Creating configured image...')
            configured_image = self.client.images.build(fileobj=docker_context_buffer,
                                                        custom_context=True,
                                                        rm=True,
                                                        forcerm=True)
            logging.info('Created image with id: %s', configured_image[0].id)
            self.images.append(configured_image)

        finally:
            conf_dockerfile_buffer.close()
            docker_context_buffer.close()

        return configured_image

    def build_image_by_path(self, dir, name=None):
        try:
            logging.info('Creating configured image...')
            configured_image = self.client.images.build(path=dir,
                                                        tag=name,
                                                        rm=True,
                                                        forcerm=True)
            logging.info('Created image with id: %s', configured_image[0].id)
            self.images.append(configured_image)
            return configured_image
        except Exception as e:
            logging.info(e)
            raise
