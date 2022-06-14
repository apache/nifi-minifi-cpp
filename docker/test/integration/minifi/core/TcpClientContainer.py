import logging
from .Container import Container


class TcpClientContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'tcp-client', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "TCP client container started"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running a tcp client docker container...')
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            network=self.network.name,
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
