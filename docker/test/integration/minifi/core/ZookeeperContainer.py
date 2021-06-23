from .Container import Container


class ZookeeperContainer(Container):
    def __init__(self, name, vols, network):
        super().__init__(name, 'zookeeper', vols, network)

    def get_startup_finish_text(self):
        return "binding to port"

    def deploy(self):
        if not self.set_deployed():
            return

        self.client.containers.run(
            self.client.images.pull("wurstmeister/zookeeper:3.4.6"),
            detach=True,
            name='zookeeper',
            network=self.network.name,
            ports={'2181/tcp': 2181})
