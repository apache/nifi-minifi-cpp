import logging
from .Container import Container


class SplunkContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'splunk', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Ansible playbook complete, will begin streaming splunkd_stderr.log"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running Splunk docker container...')
        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            network=self.network.name,
            environment=[
                "SPLUNK_LICENSE_URI=Free",
                "SPLUNK_START_ARGS=--accept-license",
                "SPLUNK_PASSWORD=splunkadmin"
            ],
            entrypoint=self.command)
        logging.info('Added container \'%s\'', self.name)
