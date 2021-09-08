import logging
from .Container import Container


class KafkaBrokerContainer(Container):
    def __init__(self, name, vols, network, image_store):
        super().__init__(name, 'kafka-broker', vols, network, image_store)

    def get_startup_finished_log_entry(self):
        return "Kafka startTimeMs"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running kafka broker docker container...')
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
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
        logging.info('Added container \'%s\'', self.name)
