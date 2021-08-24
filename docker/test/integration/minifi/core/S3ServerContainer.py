import logging
from .Container import Container


class S3ServerContainer(Container):
    def __init__(self, name, vols, network, image_store):
        super().__init__(name, 's3-server', vols, network, image_store)

    def get_startup_finished_log_entry(self):
        return "Started S3MockApplication"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running s3 server docker container...')
        self.client.containers.run(
            "adobe/s3mock:2.1.28",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'9090/tcp': 9090, '9191/tcp': 9191},
            environment=["initialBuckets=test_bucket"])
        logging.info('Added container \'%s\'', self.name)
