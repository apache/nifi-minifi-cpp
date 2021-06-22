import logging
from .FlowContainer import FlowContainer
from textwrap import dedent
from ..flow_serialization.Nifi_flow_xml_serializer import Nifi_flow_xml_serializer
from io import BytesIO
import gzip


class NifiContainer(FlowContainer):
    def __init__(self, name, vols, network):
        super().__init__(name, 'nifi', vols, network)
        self.nifi_version = '1.7.0'
        self.nifi_root = '/opt/nifi/nifi-' + self.nifi_version

    def deploy(self):
        if not self.set_deployed():
            return

        dockerfile = dedent(r"""FROM {base_image}
                USER root
                ADD flow.xml.gz {nifi_root}/conf/flow.xml.gz
                RUN chown nifi:nifi {nifi_root}/conf/flow.xml.gz
                RUN sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties
                RUN sed -i -e 's/^\(nifi.remote.input.socket.port\)=.*/\1=5000/' {nifi_root}/conf/nifi.properties
                USER nifi
                """.format(name=self.name,
                           base_image='apache/nifi:' + self.nifi_version,
                           nifi_root=self.nifi_root))

        serializer = Nifi_flow_xml_serializer()
        test_flow_xml = serializer.serialize(self.start_nodes, self.nifi_version)
        logging.info('Using generated flow config xml:\n%s', test_flow_xml)

        conf_file_buffer = BytesIO()

        try:
            with gzip.GzipFile(mode='wb', fileobj=conf_file_buffer) as conf_gz_file_buffer:
                conf_gz_file_buffer.write(test_flow_xml.encode())
            conf_file_len = conf_file_buffer.tell()
            conf_file_buffer.seek(0)

            context_files = [
                {
                    'name': 'flow.xml.gz',
                    'size': conf_file_len,
                    'file_obj': conf_file_buffer
                }
            ]

            configured_image = self.build_image(dockerfile, context_files)

        finally:
            conf_file_buffer.close()

        logging.info('Creating and running docker container for flow...')

        self.docker_container = self.client.containers.run(
            configured_image[0],
            detach=True,
            name=self.name,
            hostname=self.name,
            network=self.network.name,
            volumes=self.vols)

        logging.info('Started container \'%s\'', self.name)
