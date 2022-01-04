import logging
from .Container import Container


class ZookeeperContainer(Container):
    def __init__(self, name, vols, network, image_store, command):
        super().__init__(name, 'zookeeper', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "binding to port"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running zookeeper docker container...')
        self.client.containers.run(
            "wurstmeister/zookeeper:3.4.6",
            detach=True,
            name='zookeeper',
            network=self.network.name,
            ports={'2181/tcp': 2181},
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
