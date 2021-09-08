from .NifiContainer import NifiContainer
from .MinifiContainer import MinifiContainer
import logging
import tarfile
import docker
from io import BytesIO
from textwrap import dedent
import os


class ImageStore:
    def __init__(self):
        self.client = docker.from_env()
        self.images = dict()

    def __del__(self):
        self.cleanup()

    def cleanup(self):
        # Clean up images
        for image in self.images.values():
            logging.info('Cleaning up image: %s', image.id)
            self.client.images.remove(image.id, force=True)

    def get_image(self, container_engine):
        if container_engine == "minifi-cpp":
            return self.__get_minifi_cpp_image()
        if container_engine == "http-proxy":
            return self.__get_http_proxy_image()
        if container_engine == "nifi":
            return self.__get_nifi_image()
        if container_engine == "postgresql-server":
            return self.__get_postgresql_server_image()
        if container_engine == "kafka-broker":
            return self.__get_kafka_broker_image()

    def __get_minifi_cpp_image(self):
        if "minifi-cpp" in self.images:
            return self.images["minifi-cpp"]

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
                RUN sed -i -e 's/INFO/DEBUG/g' {minifi_root}/conf/minifi-log.properties
                USER minificpp
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_VERSION,
                           minifi_root=MinifiContainer.MINIFI_ROOT))

        self.images["minifi-cpp"] = self.__build_image(dockerfile)
        return self.images["minifi-cpp"]

    def __get_http_proxy_image(self):
        if "http-proxy" in self.images:
            return self.images["http-proxy"]

        dockerfile = dedent("""FROM {base_image}
                RUN apt -y update && apt install -y apache2-utils
                RUN htpasswd -b -c /etc/squid/.squid_users {proxy_username} {proxy_password}
                RUN echo 'auth_param basic program /usr/lib/squid3/basic_ncsa_auth /etc/squid/.squid_users'  > /etc/squid/squid.conf && \
                    echo 'auth_param basic realm proxy' >> /etc/squid/squid.conf && \
                    echo 'acl authenticated proxy_auth REQUIRED' >> /etc/squid/squid.conf && \
                    echo 'http_access allow authenticated' >> /etc/squid/squid.conf && \
                    echo 'http_port {proxy_port}' >> /etc/squid/squid.conf
                ENTRYPOINT ["/sbin/entrypoint.sh"]
                """.format(base_image='sameersbn/squid:3.5.27-2', proxy_username='admin', proxy_password='test101', proxy_port='3128'))

        self.images["http-proxy"] = self.__build_image(dockerfile)
        return self.images["http-proxy"]

    def __get_nifi_image(self):
        if "nifi" in self.images:
            return self.images["nifi"]

        dockerfile = dedent(r"""FROM {base_image}
                USER root
                RUN sed -i -e 's/^\(nifi.remote.input.host\)=.*/\1={name}/' {nifi_root}/conf/nifi.properties
                RUN sed -i -e 's/^\(nifi.remote.input.socket.port\)=.*/\1=5000/' {nifi_root}/conf/nifi.properties
                USER nifi
                """.format(name='nifi',
                           base_image='apache/nifi:' + NifiContainer.NIFI_VERSION,
                           nifi_root=NifiContainer.NIFI_ROOT))

        self.images["nifi"] = self.__build_image(dockerfile)
        return self.images["nifi"]

    def __get_postgresql_server_image(self):
        if "postgresql-server" in self.images:
            return self.images["postgresql-server"]

        dockerfile = dedent("""FROM {base_image}
                RUN mkdir -p /docker-entrypoint-initdb.d
                RUN echo "#!/bin/bash" > /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "set -e" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "psql -v ON_ERROR_STOP=1 --username "postgres" --dbname "postgres" <<-EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table (int_col INTEGER, text_col TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (1, 'apple');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (2, 'banana');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (3, 'pear');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh
                """.format(base_image='postgres:13.2'))
        self.images["postgresql-server"] = self.__build_image(dockerfile)
        return self.images["postgresql-server"]

    def __get_kafka_broker_image(self):
        if "kafka-broker" in self.images:
            return self.images["kafka-broker"]

        test_dir = os.environ['PYTHONPATH'].split(':')[-1]  # Based on DockerVerify.sh
        self.images["kafka-broker"] = self.__build_image_by_path(test_dir + "/resources/kafka_broker", 'minifi-kafka')
        return self.images["kafka-broker"]

    def __build_image(self, dockerfile, context_files=[]):
        conf_dockerfile_buffer = BytesIO()
        docker_context_buffer = BytesIO()

        try:
            # Overlay conf onto base nifi image
            conf_dockerfile_buffer.write(dockerfile.encode())
            conf_dockerfile_buffer.seek(0)

            with tarfile.open(mode='w', fileobj=docker_context_buffer) as docker_context:
                dockerfile_info = tarfile.TarInfo('Dockerfile')
                dockerfile_info.size = conf_dockerfile_buffer.getbuffer().nbytes
                docker_context.addfile(dockerfile_info,
                                       fileobj=conf_dockerfile_buffer)

                for context_file in context_files:
                    file_info = tarfile.TarInfo(context_file['name'])
                    file_info.size = context_file['size']
                    docker_context.addfile(file_info,
                                           fileobj=context_file['file_obj'])
            docker_context_buffer.seek(0)

            logging.info('Creating configured image...')
            image = self.client.images.build(fileobj=docker_context_buffer,
                                             custom_context=True,
                                             rm=True,
                                             forcerm=True)
            logging.info('Created image with id: %s', image[0].id)

        finally:
            conf_dockerfile_buffer.close()
            docker_context_buffer.close()

        return image[0]

    def __build_image_by_path(self, dir, name=None):
        try:
            logging.info('Creating configured image...')
            image = self.client.images.build(path=dir,
                                             tag=name,
                                             rm=True,
                                             forcerm=True)
            logging.info('Created image with id: %s', image[0].id)
            return image[0]
        except Exception as e:
            logging.info(e)
            raise
