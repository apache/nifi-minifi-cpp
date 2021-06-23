import logging
import os
from .Container import Container
from textwrap import dedent


class KafkaBrokerContainer(Container):
    def __init__(self, name,  vols, network):
        super().__init__(name, 'kafka-broker', vols, network)
        self.kafka_broker_root = '/opt/kafka'

    def get_startup_finish_text(self):
        return "Kafka startTimeMs"

    def get_log_file_path(self):
        return self.kafka_broker_root + '/logs/server.log'

    def deploy(self):
        if not self.set_deployed():
            return

        test_dir = os.environ['PYTHONPATH'].split(':')[-1]  # Based on DockerVerify.sh
        broker_image = self.build_image_by_path(test_dir + "/resources/kafka_broker", 'minifi-kafka')
        broker = self.client.containers.run(
            broker_image[0],
            detach=True,
            name='kafka-broker',
            network=self.network.name,
            ports={'9092/tcp': 9092, '29092/tcp': 29092},
            environment=[
                "KAFKA_BROKER_ID=1",
                'ALLOW_PLAINTEXT_LISTENER: "yes"',
                "KAFKA_LISTENERS=PLAINTEXT://kafka-broker:9092,SSL://kafka-broker:9093,PLAINTEXT_HOST://0.0.0.0:29092",
                "KAFKA_LISTENER_SECURITY_PROTOCOL_MAP=PLAINTEXT:PLAINTEXT,PLAINTEXT_HOST:PLAINTEXT,SSL:SSL",
                "KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://kafka-broker:9092,SSL://kafka-broker:9093,PLAINTEXT_HOST://localhost:29092",
                "KAFKA_ZOOKEEPER_CONNECT=zookeeper:2181"])
        logging.info('Adding container \'%s\'', broker.name)
