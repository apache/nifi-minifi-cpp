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


import logging
import tempfile
import os
import OpenSSL.crypto
import docker.types

from .Container import Container
from ssl_utils.SSL_cert_utils import make_server_cert, make_cert_without_extended_usage


class ElasticsearchContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None):
        super().__init__(feature_context, name, 'elasticsearch', vols, network, image_store, command)
        http_cert, http_key = make_server_cert(f"elasticsearch-{feature_context.id}", feature_context.root_ca_cert, feature_context.root_ca_key)
        transport_cert, transport_key = make_cert_without_extended_usage("127.0.0.1", feature_context.root_ca_cert, feature_context.root_ca_key)

        self.root_ca_file = tempfile.NamedTemporaryFile(delete=False)
        self.root_ca_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=feature_context.root_ca_cert))
        self.root_ca_file.close()
        os.chmod(self.root_ca_file.name, 0o644)

        self.http_cert_file = tempfile.NamedTemporaryFile(delete=False)
        self.http_cert_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=http_cert))
        self.http_cert_file.close()
        os.chmod(self.http_cert_file.name, 0o644)

        self.http_key_file = tempfile.NamedTemporaryFile(delete=False)
        self.http_key_file.write(OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM, pkey=http_key))
        self.http_key_file.close()
        os.chmod(self.http_key_file.name, 0o644)

        self.transport_cert_file = tempfile.NamedTemporaryFile(delete=False)
        self.transport_cert_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=transport_cert))
        self.transport_cert_file.close()
        os.chmod(self.transport_cert_file.name, 0o644)

        self.transport_key_file = tempfile.NamedTemporaryFile(delete=False)
        self.transport_key_file.write(OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM, pkey=transport_key))
        self.transport_key_file.close()
        os.chmod(self.transport_key_file.name, 0o644)

    def get_startup_finished_log_entry(self):
        return '"current.health":"GREEN"'

    def deploy(self):
        if not self.set_deployed():
            return

        mounts = [
            docker.types.Mount(
                type='bind',
                source=os.environ['TEST_DIRECTORY'] + "/resources/elasticsearch/elasticsearch.yml",
                target='/usr/share/elasticsearch/config/elasticsearch.yml'),
            docker.types.Mount(
                type='bind',
                source=self.http_key_file.name,
                target='/usr/share/elasticsearch/config/certs/elastic_http.key'),
            docker.types.Mount(
                type='bind',
                source=self.http_cert_file.name,
                target='/usr/share/elasticsearch/config/certs/elastic_http.crt'),
            docker.types.Mount(
                type='bind',
                source=self.transport_key_file.name,
                target='/usr/share/elasticsearch/config/certs/elastic_transport.key'),
            docker.types.Mount(
                type='bind',
                source=self.transport_cert_file.name,
                target='/usr/share/elasticsearch/config/certs/elastic_transport.crt'),
            docker.types.Mount(
                type='bind',
                source=self.root_ca_file.name,
                target='/usr/share/elasticsearch/config/certs/root_ca.crt')]

        logging.info('Creating and running Elasticsearch docker container...')
        self.client.containers.run(
            image="elasticsearch:9.1.5",
            detach=True,
            name=self.name,
            environment=[
                "ELASTIC_PASSWORD=password",
            ],
            network=self.network.name,
            mounts=mounts)
        logging.info('Added container \'%s\'', self.name)
