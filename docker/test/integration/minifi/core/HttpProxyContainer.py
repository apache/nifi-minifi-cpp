import logging
from .Container import Container


class HttpProxyContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'http-proxy', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Accepting HTTP Socket connections at"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running http-proxy docker container...')
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'3128/tcp': 3128},
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
