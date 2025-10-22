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
import os
from textwrap import dedent

from minifi_test_framework.containers.docker_image_builder import DockerImageBuilder
from minifi_test_framework.core.hooks import common_before_scenario
from minifi_test_framework.core.hooks import common_after_scenario
from minifi_test_framework.core.hooks import get_minifi_container_image


def before_all(context):
    minifi_tag_prefix = os.environ['MINIFI_TAG_PREFIX']
    if "rocky" in minifi_tag_prefix:
        install_sql_cmd = "dnf -y install postgresql-odbc"
        so_location = "psqlodbca.so"
    elif "bullseye" in minifi_tag_prefix or "bookworm" in minifi_tag_prefix or "trixie" in minifi_tag_prefix:
        install_sql_cmd = "apt -y install odbc-postgresql"
        so_location = "/usr/lib/$(gcc -dumpmachine)/odbc/psqlodbca.so"
    elif "jammy" in minifi_tag_prefix or "noble" in minifi_tag_prefix:
        install_sql_cmd = "apt -y install odbc-postgresql"
        so_location = "/usr/lib/$(gcc -dumpmachine)/odbc/psqlodbca.so"
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
            """.format(base_image=get_minifi_container_image(),
                       install_sql_cmd=install_sql_cmd, so_location=so_location))
    builder = DockerImageBuilder(
        image_tag="apacheminificpp-sql:behave",
        dockerfile_content=dockerfile
    )
    builder.build()


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp-sql:behave"
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
