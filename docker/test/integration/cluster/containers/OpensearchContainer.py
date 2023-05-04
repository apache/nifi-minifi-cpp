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
import os
import tempfile

import OpenSSL.crypto
import docker.types

from .Container import Container
from ssl_utils.SSL_cert_utils import make_server_cert


class OpensearchContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None):
        super().__init__(feature_context, name, 'opensearch', vols, network, image_store, command)
        cert, key = make_server_cert(f"opensearch-{feature_context.id}", feature_context.root_ca_cert, feature_context.root_ca_key)

        self.root_ca_file = tempfile.NamedTemporaryFile(delete=False)
        self.root_ca_file.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=feature_context.root_ca_cert))
        self.root_ca_file.close()
        os.chmod(self.root_ca_file.name, 0o644)

        self.admin_pem = tempfile.NamedTemporaryFile(delete=False)
        self.admin_pem.write(OpenSSL.crypto.dump_certificate(type=OpenSSL.crypto.FILETYPE_PEM, cert=cert))
        self.admin_pem.close()
        os.chmod(self.admin_pem.name, 0o644)

        self.admin_key = tempfile.NamedTemporaryFile(delete=False)
        self.admin_key.write(OpenSSL.crypto.dump_privatekey(type=OpenSSL.crypto.FILETYPE_PEM, pkey=key))
        self.admin_key.close()
        os.chmod(self.admin_key.name, 0o644)

    def get_startup_finished_log_entry(self):
        return 'Hot-reloading of audit configuration is enabled'

    def deploy(self):
        if not self.set_deployed():
            return
        mounts = [
            docker.types.Mount(
                type='bind',
                source=os.environ['TEST_DIRECTORY'] + "/resources/opensearch/opensearch.yml",
                target='/usr/share/opensearch/config/opensearch.yml'),
            docker.types.Mount(
                type='bind',
                source=self.admin_pem.name,
                target='/usr/share/opensearch/config/admin.pem'),
            docker.types.Mount(
                type='bind',
                source=self.admin_key.name,
                target='/usr/share/opensearch/config/admin-key.pem'),
            docker.types.Mount(
                type='bind',
                source=self.root_ca_file.name,
                target='/usr/share/opensearch/config/root-ca.pem')]

        logging.info('Creating and running Opensearch docker container...')
        self.client.containers.run(
            image='opensearchproject/opensearch:2.6.0',
            detach=True,
            name=self.name,
            network=self.network.name,
            mounts=mounts)

        logging.info('Added container \'%s\'', self.name)
