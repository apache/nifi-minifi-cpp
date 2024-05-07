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


import docker
import logging
import os
import shutil

from ..LogSource import LogSource
from .MinifiContainer import MinifiContainer


class MinifiAsPodInKubernetesCluster(MinifiContainer):
    MINIFI_IMAGE_NAME = 'apacheminificpp'
    MINIFI_IMAGE_TAG = 'docker_test'

    def __init__(self, feature_context, kubernetes_proxy, config_dir, minifi_options, name, vols, network, image_store, command=None):
        test_dir = os.environ['TEST_DIRECTORY']
        shutil.copy(os.path.join(test_dir, os.pardir, os.pardir, os.pardir, 'conf', 'minifi.properties'), config_dir)
        shutil.copy(os.path.join(test_dir, 'resources', 'kubernetes', 'minifi-conf', 'minifi-log.properties'), config_dir)
        super().__init__(feature_context=feature_context,
                         config_dir=config_dir,
                         options=minifi_options,
                         name=name,
                         vols=vols,
                         network=network,
                         image_store=image_store,
                         command=command)

        self.kubernetes_proxy = kubernetes_proxy

        docker_client = docker.from_env()
        minifi_image = docker_client.images.get(MinifiAsPodInKubernetesCluster.MINIFI_IMAGE_NAME + ':' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION)
        minifi_image.tag(MinifiAsPodInKubernetesCluster.MINIFI_IMAGE_NAME, MinifiAsPodInKubernetesCluster.MINIFI_IMAGE_TAG)

    def _create_container_config_dir(self, config_dir):
        return config_dir

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Setting up container: %s', self.name)

        self._create_config()
        self.kubernetes_proxy.create_helper_objects()
        self.kubernetes_proxy.load_docker_image(MinifiAsPodInKubernetesCluster.MINIFI_IMAGE_NAME, MinifiAsPodInKubernetesCluster.MINIFI_IMAGE_TAG)
        self.kubernetes_proxy.create_minifi_pod()

        logging.info('Finished setting up container: %s', self.name)

    def log_source(self):
        return LogSource.FROM_GET_APP_LOG_METHOD

    def get_app_log(self):
        return 'OK', self.kubernetes_proxy.get_logs('daemon', 'minifi')

    def cleanup(self):
        # cleanup is done through the kubernetes cluster in the environment.py
        pass
