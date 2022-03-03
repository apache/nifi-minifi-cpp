import logging
import os
from .Container import Container


class FakeGcsServerContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'fake-gcs-server', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "server started at http"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running google cloud storage server docker container...')
        self.client.containers.run(
            "fsouza/fake-gcs-server:latest",
            detach=True,
            name=self.name,
            network=self.network.name,
            entrypoint=self.command,
            ports={'4443/tcp': 4443},
            volumes=[os.environ['TEST_DIRECTORY'] + "/resources/fake-gcs-server-data:/data"],
            command='-scheme http -host fake-gcs-server')
        logging.info('Added container \'%s\'', self.name)
