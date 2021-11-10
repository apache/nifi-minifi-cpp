import logging
from .Container import Container


class OPCUAServerContainer(Container):
    def __init__(self, name, vols, network, image_store):
        super().__init__(name, 'opcua-server', vols, network, image_store)

    def get_startup_finished_log_entry(self):
        return "TCP network layer listening on"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running OPC UA server docker container...')
        self.client.containers.run(
            "lordgamez/open62541",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'4840/tcp': 4840})
        logging.info('Added container \'%s\'', self.name)
