#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

import os
from textwrap import dedent

from containers.postgress_server_container import PostgresContainer
from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.hooks import (
    common_after_scenario,
    common_before_scenario,
    get_minifi_container_image,
)

import docker

# These hooks are executed by behave before and after each scenario
# The common_before_scenario and common_after_scenario must be called for proper setup and tear down


def before_all(context):
    minifi_tag_prefix = os.environ.get("MINIFI_TAG_PREFIX", "")
    if "rocky" in minifi_tag_prefix:
        install_sql_cmd = "dnf -y install postgresql-odbc"
        so_location = "psqlodbca.so"
    elif (
        "bullseye" in minifi_tag_prefix
        or "bookworm" in minifi_tag_prefix
        or "trixie" in minifi_tag_prefix
        or "jammy" in minifi_tag_prefix
        or "noble" in minifi_tag_prefix
    ):
        install_sql_cmd = "apt -y install odbc-postgresql"
        so_location = "/usr/lib/$(gcc -dumpmachine)/odbc/psqlodbca.so"
    else:
        install_sql_cmd = "apk --update --no-cache add psqlodbc"
        so_location = "psqlodbca.so"
    dockerfile = dedent(
        f"""\
            FROM {get_minifi_container_image()}
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
            """
    )
    builder = DockerImageBuilder(image_tag="apacheminificpp-sql:behave", dockerfile_content=dockerfile)
    builder.build()
    docker.from_env().images.pull(PostgresContainer.IMAGE)


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp-sql:behave"
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
