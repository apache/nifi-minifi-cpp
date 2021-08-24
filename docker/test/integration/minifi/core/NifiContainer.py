import logging

from .FlowContainer import FlowContainer
from ..flow_serialization.Nifi_flow_xml_serializer import Nifi_flow_xml_serializer
import gzip
import os


class NifiContainer(FlowContainer):
    NIFI_VERSION = '1.7.0'
    NIFI_ROOT = '/opt/nifi/nifi-' + NIFI_VERSION

    def __init__(self, config_dir, name, vols, network, image_store):
        super().__init__(config_dir, name, 'nifi', vols, network, image_store)

    def get_startup_finished_log_entry(self):
        return "Starting Flow Controller"

    def get_log_file_path(self):
        return NifiContainer.NIFI_ROOT + '/logs/nifi-app.log'

    def __create_config(self):
        serializer = Nifi_flow_xml_serializer()
        test_flow_xml = serializer.serialize(self.start_nodes, NifiContainer.NIFI_VERSION)
        logging.info('Using generated flow config xml:\n%s', test_flow_xml)

        with gzip.open(os.path.join(self.config_dir, "flow.xml.gz"), 'wb') as gz_file:
            gz_file.write(test_flow_xml.encode())

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running nifi docker container...')
        self.__create_config()

        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            hostname=self.name,
            network=self.network.name,
            entrypoint=["/bin/sh", "-c", "cp /tmp/nifi_config/flow.xml.gz " + NifiContainer.NIFI_ROOT + "/conf && /opt/nifi/scripts/start.sh"],
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
