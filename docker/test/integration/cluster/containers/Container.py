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

from ..LogSource import LogSource
from ..FeatureContext import FeatureContext


class Container:
    def __init__(self, feature_context: FeatureContext, name, engine, vols, network, image_store, command):
        self.feature_context = feature_context
        self.name = name
        self.engine = engine
        self.vols = vols
        self.network = network
        self.image_store = image_store
        self.command = command

        # Get docker client
        self.client = docker.from_env()
        self.deployed = False
        self.post_startup_commands_finished = False

    def cleanup(self):
        logging.info('Cleaning up container: %s', self.name)
        try:
            self.client.containers.get(self.name).remove(v=True, force=True)
        except docker.errors.NotFound:
            logging.warning("Container '%s' has been cleaned up already, nothing to be done.", self.name)
            pass

    def set_deployed(self):
        if self.deployed:
            return False
        self.deployed = True
        return True

    def get_name(self):
        return self.name

    def get_engine(self):
        return self.engine

    def deploy(self):
        raise NotImplementedError()

    def log_source(self):
        return LogSource.FROM_DOCKER_CONTAINER

    def stop(self):
        logging.info('Stopping docker container "%s"...', self.name)
        self.client.containers.get(self.name).stop()
        logging.info('Successfully stopped docker container "%s"', self.name)
        self.deployed = False

    def kill(self):
        logging.info('Killing docker container "%s"...', self.name)
        self.client.containers.get(self.name).kill()
        logging.info('Successfully killed docker container "%s"', self.name)
        self.deployed = False

    def restart(self):
        logging.info('Restarting docker container "%s"...', self.name)
        self.client.containers.get(self.name).restart()
        logging.info('Successfully restarted docker container "%s"', self.name)
        self.deployed = True

    def get_startup_finished_log_entry(self):
        raise NotImplementedError()

    def get_app_log(self):
        raise NotImplementedError()

    def run_post_startup_commands(self):
        return True
