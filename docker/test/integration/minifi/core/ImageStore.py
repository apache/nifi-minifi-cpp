# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


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
        self.test_dir = os.environ['TEST_DIRECTORY']  # Based on DockerVerify.sh

    def __del__(self):
        self.cleanup()

    def cleanup(self):
        # Clean up images
        for image in self.images.values():
            logging.info('Cleaning up image: %s', image.id)
            self.client.images.remove(image.id, force=True)

    def get_image(self, container_engine):
        if container_engine in self.images:
            return self.images[container_engine]

        if container_engine == "minifi-cpp" or container_engine == "transient-minifi":
            image = self.__build_minifi_cpp_image()
        elif container_engine == "minifi-cpp-with-provenance-repo":
            image = self.__build_minifi_cpp_image_with_provenance_repo()
        elif container_engine == "minifi-cpp-with-https-c2-config":
            image = self.__build_minifi_cpp_image_with_https_c2_config()
        elif container_engine == "http-proxy":
            image = self.__build_http_proxy_image()
        elif container_engine == "postgresql-server":
            image = self.__build_postgresql_server_image()
        elif container_engine == "kafka-broker":
            image = self.__build_kafka_broker_image()
        elif container_engine == "mqtt-broker":
            image = self.__build_mqtt_broker_image()
        elif container_engine == "splunk":
            image = self.__build_splunk_image()
        elif container_engine == "tcp-client":
            image = self.__build_tcp_client_image()
        elif container_engine == "prometheus":
            image = self.__build_prometheus_image()
        elif container_engine == "elasticsearch":
            image = self.__build_elasticsearch_image()
        elif container_engine == "opensearch":
            image = self.__build_opensearch_image()
        elif container_engine == "minifi-c2-server":
            image = self.__build_minifi_c2_image()
        elif container_engine == "minifi-c2-server-ssl":
            image = self.__build_minifi_c2_ssl_image()
        else:
            raise Exception("There is no associated image for " + container_engine)

        self.images[container_engine] = image
        return image

    def __build_minifi_cpp_image(self):
        dockerfile = dedent("""\
                FROM {base_image}
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
                RUN sed -i -e 's/INFO/TRACE/g' {minifi_root}/conf/minifi-log.properties
                RUN echo nifi.flow.engine.threads=5 >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.metrics.publisher.agent.identifier=Agent1 >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.metrics.publisher.class=PrometheusMetricsPublisher >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.metrics.publisher.PrometheusMetricsPublisher.port=9936 >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.metrics.publisher.metrics=RepositoryMetrics,QueueMetrics,GetFileMetrics,GetTCPMetrics,PutFileMetrics,FlowInformation,DeviceInfoNode,AgentStatus >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.enable=true  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.rest.url=http://minifi-c2-server:10090/c2/config/heartbeat  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.rest.url.ack=http://minifi-c2-server:10090/c2/config/acknowledge  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.flow.base.url=http://minifi-c2-server:10090/c2/config/  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.root.classes=DeviceInfoNode,AgentInformation,FlowInformation  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.full.heartbeat=false  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.agent.class=minifi-test-class  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.agent.identifier=minifi-test-id  >> {minifi_root}/conf/minifi.properties
                USER minificpp
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_VERSION,
                           minifi_root=MinifiContainer.MINIFI_ROOT))

        return self.__build_image(dockerfile)

    def __build_minifi_cpp_image_with_provenance_repo(self):
        dockerfile = dedent("""\
                FROM {base_image}
                USER root
                COPY minifi.properties {minifi_root}/conf/minifi.properties
                USER minificpp
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_VERSION,
                           minifi_root=MinifiContainer.MINIFI_ROOT))

        properties_path = self.test_dir + "/resources/minifi_cpp_with_provenance_repo/minifi.properties"
        properties_context = {'name': 'minifi.properties', 'size': os.path.getsize(properties_path)}

        with open(properties_path, 'rb') as properties_file:
            properties_context['file_obj'] = properties_file
            image = self.__build_image(dockerfile, [properties_context])
        return image

    def __build_minifi_cpp_image_with_https_c2_config(self):
        dockerfile = dedent("""\
                FROM {base_image}
                USER root
                RUN sed -i -e 's/INFO/DEBUG/g' {minifi_root}/conf/minifi-log.properties
                RUN echo nifi.c2.enable=true  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.rest.url=https://minifi-c2-server:10090/c2/config/heartbeat  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.rest.url.ack=https://minifi-c2-server:10090/c2/config/acknowledge  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.rest.ssl.context.service=SSLContextService  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.flow.base.url=https://minifi-c2-server:10090/c2/config/  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.full.heartbeat=false  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.agent.class=minifi-test-class  >> {minifi_root}/conf/minifi.properties
                RUN echo nifi.c2.agent.identifier=minifi-test-id  >> {minifi_root}/conf/minifi.properties
                USER minificpp
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_VERSION,
                           minifi_root=MinifiContainer.MINIFI_ROOT))

        return self.__build_image(dockerfile)

    def __build_http_proxy_image(self):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN apt -y update && apt install -y apache2-utils
                RUN htpasswd -b -c /etc/squid/.squid_users {proxy_username} {proxy_password}
                RUN echo 'auth_param basic program /usr/lib/squid3/basic_ncsa_auth /etc/squid/.squid_users'  > /etc/squid/squid.conf && \
                    echo 'auth_param basic realm proxy' >> /etc/squid/squid.conf && \
                    echo 'acl authenticated proxy_auth REQUIRED' >> /etc/squid/squid.conf && \
                    echo 'http_access allow authenticated' >> /etc/squid/squid.conf && \
                    echo 'http_port {proxy_port}' >> /etc/squid/squid.conf
                ENTRYPOINT ["/sbin/entrypoint.sh"]
                """.format(base_image='sameersbn/squid:3.5.27-2', proxy_username='admin', proxy_password='test101', proxy_port='3128'))

        return self.__build_image(dockerfile)

    def __build_postgresql_server_image(self):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN mkdir -p /docker-entrypoint-initdb.d
                RUN echo "#!/bin/bash" > /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "set -e" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "psql -v ON_ERROR_STOP=1 --username "postgres" --dbname "postgres" <<-EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table (int_col INTEGER, text_col TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (1, 'apple');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (2, 'banana');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (3, 'pear');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table2 (int_col INTEGER, \\"tExT_Col\\" TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table2 (int_col, \\"tExT_Col\\") VALUES (5, 'ApPlE');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table2 (int_col, \\"tExT_Col\\") VALUES (6, 'BaNaNa');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh
                """.format(base_image='postgres:13.2'))
        return self.__build_image(dockerfile)

    def __build_kafka_broker_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/kafka_broker", 'minifi-kafka')

    def __build_mqtt_broker_image(self):
        dockerfile = dedent("""\
            FROM {base_image}
            RUN echo 'log_dest stderr' >> /mosquitto-no-auth.conf
            CMD ["/usr/sbin/mosquitto", "--verbose", "--config-file", "/mosquitto-no-auth.conf"]
            """.format(base_image='eclipse-mosquitto:2.0.14'))

        return self.__build_image(dockerfile)

    def __build_splunk_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/splunk-hec", 'minifi-splunk')

    def __build_tcp_client_image(self):
        dockerfile = dedent("""\
            FROM {base_image}
            RUN apk add netcat-openbsd
            CMD ["/bin/sh", "-c", "echo TCP client container started; while true; do echo test_tcp_message | nc minifi-cpp-flow 10254; sleep 1; done"]
            """.format(base_image='alpine:3.13'))

        return self.__build_image(dockerfile)

    def __build_prometheus_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/prometheus", 'minifi-prometheus')

    def __build_elasticsearch_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/elasticsearch", 'elasticsearch')

    def __build_opensearch_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/opensearch", 'opensearch')

    def __build_minifi_c2_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/minifi-c2-server", 'minifi-c2-server')

    def __build_minifi_c2_ssl_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/minifi-c2-server-ssl", 'minifi-c2-server')

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
