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


from .containers.MinifiContainer import MinifiContainer
from .containers.NifiContainer import NifiContainer
import logging
import tarfile
import docker
from io import BytesIO
from textwrap import dedent
import os


class PythonOptions:
    REQUIREMENTS_FILE = 0
    SYSTEM_INSTALLED_PACKAGES = 1
    INLINE_DEFINED_PACKAGES = 2


class ImageStore:
    def __init__(self):
        self.client = docker.from_env()
        self.images = dict()
        self.test_dir = os.environ['TEST_DIRECTORY']  # Based on DockerVerify.sh

    def cleanup(self):
        # Clean up images
        for image in self.images.values():
            logging.info('Cleaning up image: %s', image.id)
            self.client.images.remove(image.id, force=True)

    def get_image(self, container_engine):
        if container_engine in self.images:
            return self.images[container_engine]

        if container_engine == "minifi-cpp-sql":
            image = self.__build_minifi_cpp_sql_image()
        elif container_engine == "minifi-cpp-with-example-python-processors":
            image = self.__build_minifi_cpp_image_with_example_minifi_python_processors()
        elif container_engine == "minifi-cpp-nifi-python":
            image = self.__build_minifi_cpp_image_with_nifi_python_processors(PythonOptions.REQUIREMENTS_FILE)
        elif container_engine == "minifi-cpp-nifi-python-system-python-packages":
            image = self.__build_minifi_cpp_image_with_nifi_python_processors(PythonOptions.SYSTEM_INSTALLED_PACKAGES)
        elif container_engine == "minifi-cpp-nifi-with-inline-python-dependencies":
            image = self.__build_minifi_cpp_image_with_nifi_python_processors(PythonOptions.INLINE_DEFINED_PACKAGES)
        elif container_engine == "http-proxy":
            image = self.__build_http_proxy_image()
        elif container_engine == "postgresql-server":
            image = self.__build_postgresql_server_image()
        elif container_engine == "mqtt-broker":
            image = self.__build_mqtt_broker_image()
        elif container_engine == "splunk":
            image = self.__build_splunk_image()
        elif container_engine == "reverse-proxy":
            image = self.__build_reverse_proxy_image()
        else:
            raise Exception("There is no associated image for " + container_engine)

        self.images[container_engine] = image
        return image

    def __build_minifi_cpp_sql_image(self):
        if "rocky" in MinifiContainer.MINIFI_TAG_PREFIX:
            install_sql_cmd = "dnf -y install postgresql-odbc"
            so_location = "psqlodbca.so"
        elif "bullseye" in MinifiContainer.MINIFI_TAG_PREFIX or "bookworm" in MinifiContainer.MINIFI_TAG_PREFIX:
            install_sql_cmd = "apt -y install odbc-postgresql"
            so_location = "/usr/lib/x86_64-linux-gnu/odbc/psqlodbca.so"
        elif "jammy" in MinifiContainer.MINIFI_TAG_PREFIX or "noble" in MinifiContainer.MINIFI_TAG_PREFIX:
            install_sql_cmd = "apt -y install odbc-postgresql"
            so_location = "/usr/lib/x86_64-linux-gnu/odbc/psqlodbca.so"
        else:
            install_sql_cmd = "apk --update --no-cache add psqlodbc"
            so_location = "psqlodbca.so"
        dockerfile = dedent("""\
                FROM {base_image}
                USER root
                RUN {install_sql_cmd}
                RUN echo "[PostgreSQL ANSI]" > /odbcinst.ini.template && \
                    echo "Description=PostgreSQL ODBC driver (ANSI version)" >> /odbcinst.ini.template && \
                    echo "Driver={so_location}" >> /odbcinst.ini.template && \
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
                USER minificpp
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION,
                           install_sql_cmd=install_sql_cmd, so_location=so_location))

        return self.__build_image(dockerfile)

    def __build_minifi_cpp_image_with_example_minifi_python_processors(self):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN cp -r /opt/minifi/minifi-current/minifi-python-examples /opt/minifi/minifi-current/minifi-python/examples
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION))

        return self.__build_image(dockerfile)

    def __build_minifi_cpp_image_with_nifi_python_processors(self, python_option):
        parse_document_url = "https://raw.githubusercontent.com/apache/nifi/rel/nifi-" + NifiContainer.NIFI_VERSION + "/nifi-python-extensions/nifi-text-embeddings-module/src/main/python/ParseDocument.py"
        chunk_document_url = "https://raw.githubusercontent.com/apache/nifi/rel/nifi-" + NifiContainer.NIFI_VERSION + "/nifi-python-extensions/nifi-text-embeddings-module/src/main/python/ChunkDocument.py"
        pip3_install_command = ""
        requirements_install_command = ""
        additional_cmd = ""
        # The following sed command is used to remove the existing dependencies from the ParseDocument and ChunkDocument processors
        # /class ProcessorDetails:/,/^$/: Do the following between 'class ProcessorDetails:' and the first empty line (so we don't modify other PropertyDescriptor blocks below)
        # /^\s*dependencies\s*=/,/\]\s*$/: Do the following between 'dependencies =' at the start of a line, and ']' at the end of a line
        # d: Delete line
        parse_document_sed_cmd = 'sed -i "/class ProcessorDetails:/,/^$/{/^\\s*dependencies\\s*=/,/\\]\\s*$/d}" /opt/minifi/minifi-current/minifi-python/nifi_python_processors/ParseDocument.py && \\'
        chunk_document_sed_cmd = 'sed -i "/class ProcessorDetails:/,/^$/{/^\\s*dependencies\\s*=/,/\\]\\s*$/d}" /opt/minifi/minifi-current/minifi-python/nifi_python_processors/ChunkDocument.py && \\'
        if python_option == PythonOptions.SYSTEM_INSTALLED_PACKAGES:
            if not MinifiContainer.MINIFI_TAG_PREFIX:
                additional_cmd = "RUN pip3 install --break-system-packages 'langchain<=0.17.0'"
            else:
                additional_cmd = "RUN pip3 install 'langchain<=0.17.0'"
        elif python_option == PythonOptions.REQUIREMENTS_FILE:
            requirements_install_command = "echo 'langchain<=0.17.0' > /opt/minifi/minifi-current/minifi-python/nifi_python_processors/requirements.txt && \\"
        elif python_option == PythonOptions.INLINE_DEFINED_PACKAGES:
            parse_document_sed_cmd = parse_document_sed_cmd[:-2] + ' sed -i "54 i \\ \\ \\ \\ \\ \\ \\ \\ dependencies = [\\\"langchain<=0.17.0\\\"]" /opt/minifi/minifi-current/minifi-python/nifi_python_processors/ParseDocument.py && \\'
            chunk_document_sed_cmd = 'sed -i "s/\\[\\\'langchain\\\'\\]/\\[\\\'langchain<=0.17.0\\\'\\]/" /opt/minifi/minifi-current/minifi-python/nifi_python_processors/ChunkDocument.py && \\'
        if not MinifiContainer.MINIFI_TAG_PREFIX:
            pip3_install_command = "RUN apk --update --no-cache add py3-pip"
        dockerfile = dedent("""\
                FROM {base_image}
                USER root
                {pip3_install_command}
                {additional_cmd}
                USER minificpp
                COPY RotatingForwarder.py /opt/minifi/minifi-current/minifi-python/nifi_python_processors/RotatingForwarder.py
                COPY SpecialPropertyTypeChecker.py /opt/minifi/minifi-current/minifi-python/nifi_python_processors/SpecialPropertyTypeChecker.py
                COPY ProcessContextInterfaceChecker.py /opt/minifi/minifi-current/minifi-python/nifi_python_processors/ProcessContextInterfaceChecker.py
                RUN wget {parse_document_url} --directory-prefix=/opt/minifi/minifi-current/minifi-python/nifi_python_processors && \\
                    wget {chunk_document_url} --directory-prefix=/opt/minifi/minifi-current/minifi-python/nifi_python_processors && \\
                    echo 'langchain<=0.17.0' > /opt/minifi/minifi-current/minifi-python/nifi_python_processors/requirements.txt && \\
                    {requirements_install_command}
                    {parse_document_sed_cmd}
                    {chunk_document_sed_cmd}
                    python3 -m venv /opt/minifi/minifi-current/venv && \\
                    python3 -m venv /opt/minifi/minifi-current/venv-with-langchain && \\
                    . /opt/minifi/minifi-current/venv-with-langchain/bin/activate && python3 -m pip install --no-cache-dir "langchain<=0.17.0" && \\
                    deactivate
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION,
                           pip3_install_command=pip3_install_command,
                           parse_document_url=parse_document_url,
                           chunk_document_url=chunk_document_url,
                           additional_cmd=additional_cmd,
                           requirements_install_command=requirements_install_command,
                           parse_document_sed_cmd=parse_document_sed_cmd,
                           chunk_document_sed_cmd=chunk_document_sed_cmd))

        return self.__build_image(dockerfile, [os.path.join(self.test_dir, "resources", "python", "RotatingForwarder.py"),
                                               os.path.join(self.test_dir, "resources", "python", "SpecialPropertyTypeChecker.py"),
                                               os.path.join(self.test_dir, "resources", "python", "ProcessContextInterfaceChecker.py")])

    def __build_http_proxy_image(self):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN apt -y update && apt install -y apache2-utils
                RUN htpasswd -b -c /etc/squid/.squid_users {proxy_username} {proxy_password}
                RUN echo 'auth_param basic program /usr/lib/squid/basic_ncsa_auth /etc/squid/.squid_users'  > /etc/squid/squid.conf && \
                    echo 'auth_param basic realm proxy' >> /etc/squid/squid.conf && \
                    echo 'acl authenticated proxy_auth REQUIRED' >> /etc/squid/squid.conf && \
                    echo 'http_access allow authenticated' >> /etc/squid/squid.conf && \
                    echo 'http_port {proxy_port}' >> /etc/squid/squid.conf
                """.format(base_image='ubuntu/squid:5.2-22.04_beta', proxy_username='admin', proxy_password='test101', proxy_port='3128'))

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

    def __build_mqtt_broker_image(self):
        dockerfile = dedent("""\
            FROM {base_image}
            RUN echo 'log_dest stderr' >> /mosquitto-no-auth.conf
            CMD ["/usr/sbin/mosquitto", "--verbose", "--config-file", "/mosquitto-no-auth.conf"]
            """.format(base_image='eclipse-mosquitto:2.0.14'))

        return self.__build_image(dockerfile)

    def __build_splunk_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/splunk-hec", 'minifi-splunk')

    def __build_reverse_proxy_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/reverse-proxy", 'reverse-proxy')

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

                for context_file_path in context_files:
                    with open(context_file_path, 'rb') as file:
                        file_info = tarfile.TarInfo(os.path.basename(context_file_path))
                        file_info.size = os.path.getsize(context_file_path)
                        docker_context.addfile(file_info, file)
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

    def get_minifi_image_python_version(self):
        result = self.client.containers.run(
            image='apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION,
            command=['python3', '-c', 'import platform; print(platform.python_version())'],
            remove=True
        )

        python_ver_str = result.decode('utf-8')
        logging.info('MiNiFi python version: %s', python_ver_str)
        return tuple(map(int, python_ver_str.split('.')))

    def is_conda_available_in_minifi_image(self):
        container = self.client.containers.create(
            image='apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION,
            command=['conda', '--version'],
        )
        try:
            container.start()
            result = container.logs()
            container.remove(force=True)
        except docker.errors.APIError:
            container.remove(force=True)
            return False

        return result.decode('utf-8').startswith('conda ')
