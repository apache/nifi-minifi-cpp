import logging
from .Container import Container


class SyslogUdpClientContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'syslog-udp-client', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Syslog UDP client started"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running a Syslog udp client docker container...')
        self.client.containers.run(
            "ubuntu:20.04",
            detach=True,
            name=self.name,
            network=self.network.name,
            entrypoint='/bin/bash -c "echo Syslog UDP client started; while true; do logger --udp -n minifi-cpp-flow -P 514 sample_log; sleep 1; done"')
        logging.info('Added container \'%s\'', self.name)
