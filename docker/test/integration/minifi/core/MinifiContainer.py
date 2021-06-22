import os
import logging
from .FlowContainer import FlowContainer
from textwrap import dedent
from ..flow_serialization.Minifi_flow_yaml_serializer import Minifi_flow_yaml_serializer
from io import BytesIO


class MinifiContainer(FlowContainer):
    def __init__(self, name, vols, network):
        super().__init__(name, 'minifi-cpp', vols, network)
        self.minifi_version = os.environ['MINIFI_VERSION']
        self.minifi_root = '/opt/minifi/nifi-minifi-cpp-' + self.minifi_version

    def deploy(self):
        if not self.set_deployed():
            return

        # Build configured image
        dockerfile = dedent("""FROM {base_image}
                USER root
                RUN apk --update --no-cache add psqlodbc
                RUN echo "[PostgreSQL ANSI]" > /odbcinst.ini.template && \
                    echo "Description=PostgreSQL ODBC driver (ANSI version)" >> /odbcinst.ini.template && \
                    echo "Driver=psqlodbca.so" >> /odbcinst.ini.template && \
                    echo "Setup=libodbcpsqlS.so" >> /odbcinst.ini.template && \
                    echo "Debug=0" >> /odbcinst.ini.template && \
                    echo "CommLog=1" >> /odbcinst.ini.template && \
                    echo "UsageCount=1" >> /odbcinst.ini.template && \
                    echo "" >> /odbcinst.ini.template && \
                    echo "[PostgreSQL Unicode]" >> /odbcinst.ini.template && \
                    echo "Description=PostgreSQL ODBC driver (Unicode version)" >> /odbcinst.ini.template && \
                    echo "Driver=psqlodbcw.so" >> /odbcinst.ini.template && \
                    echo "Setup=libodbcpsqlS.so" >> /odbcinst.ini.template && \
                    echo "Debug=0" >> /odbcinst.ini.template && \
                    echo "CommLog=1" >> /odbcinst.ini.template && \
                    echo "UsageCount=1" >> /odbcinst.ini.template
                RUN odbcinst -i -d -f /odbcinst.ini.template
                RUN echo "[ODBC]" > /etc/odbc.ini && \
                    echo "Driver = PostgreSQL ANSI" >> /etc/odbc.ini && \
                    echo "Description = PostgreSQL Data Source" >> /etc/odbc.ini && \
                    echo "Servername = postgres" >> /etc/odbc.ini && \
                    echo "Port = 5432" >> /etc/odbc.ini && \
                    echo "Protocol = 8.4" >> /etc/odbc.ini && \
                    echo "UserName = postgres" >> /etc/odbc.ini && \
                    echo "Password = password" >> /etc/odbc.ini && \
                    echo "Database = postgres" >> /etc/odbc.ini
                ADD config.yml {minifi_root}/conf/config.yml
                RUN chown minificpp:minificpp {minifi_root}/conf/config.yml
                RUN sed -i -e 's/INFO/DEBUG/g' {minifi_root}/conf/minifi-log.properties
                USER minificpp
                """.format(base_image='apacheminificpp:' + self.minifi_version,
                           minifi_root=self.minifi_root))

        serializer = Minifi_flow_yaml_serializer()
        test_flow_yaml = serializer.serialize(self.start_nodes)
        logging.info('Using generated flow config yml:\n%s', test_flow_yaml)

        conf_file_buffer = BytesIO()

        try:
            conf_file_buffer.write(test_flow_yaml.encode('utf-8'))
            conf_file_len = conf_file_buffer.tell()
            conf_file_buffer.seek(0)

            context_files = [
                {
                    'name': 'config.yml',
                    'size': conf_file_len,
                    'file_obj': conf_file_buffer
                }
            ]

            configured_image = self.build_image(dockerfile, context_files)

        finally:
            conf_file_buffer.close()

        self.docker_container = self.client.containers.run(
            configured_image[0],
            detach=True,
            name=self.name,
            network=self.network.name,
            volumes=self.vols)

        logging.info('Started container \'%s\'', self.docker_container.name)
