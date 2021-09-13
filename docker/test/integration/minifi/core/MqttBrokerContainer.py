import logging
from .Container import Container


class MqttBrokerContainer(Container):
    def __init__(self, name, vols, network, image_store):
        super().__init__(name, 'mqtt-broker', vols, network, image_store)

    def get_startup_finished_log_entry(self):
        return "mosquitto version 2.0.11 running"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running MQTT broker docker container...')
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'1883/tcp': 1883})
        logging.info('Added container \'%s\'', self.name)
