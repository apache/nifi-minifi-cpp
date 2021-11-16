import os
import logging
from .FlowContainer import FlowContainer
from ..flow_serialization.Minifi_flow_yaml_serializer import Minifi_flow_yaml_serializer


class MinifiContainer(FlowContainer):
    MINIFI_VERSION = os.environ['MINIFI_VERSION']
    MINIFI_ROOT = '/opt/minifi/nifi-minifi-cpp-' + MINIFI_VERSION

    def __init__(self, config_dir, name, vols, network, image_store, command=None):
        if not command:
            command = ["/bin/sh", "-c", "cp /tmp/minifi_config/config.yml " + MinifiContainer.MINIFI_ROOT + "/conf && /opt/minifi/minifi-current/bin/minifi.sh run"]
        super().__init__(config_dir, name, 'minifi-cpp', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "Starting Flow Controller"

    def get_log_file_path(self):
        return MinifiContainer.MINIFI_ROOT + '/logs/minifi-app.log'

    def __create_config(self):
        serializer = Minifi_flow_yaml_serializer()
        test_flow_yaml = serializer.serialize(self.start_nodes)
        logging.info('Using generated flow config yml:\n%s', test_flow_yaml)
        with open(os.path.join(self.config_dir, "config.yml"), 'wb') as config_file:
            config_file.write(test_flow_yaml.encode('utf-8'))

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running minifi docker container...')
        self.__create_config()

        self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name=self.name,
            network=self.network.name,
            entrypoint=self.command,
            volumes=self.vols)
        logging.info('Added container \'%s\'', self.name)
