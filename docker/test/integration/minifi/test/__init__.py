# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the \"License\"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an \"AS IS\" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
import shutil
import uuid
from threading import Event

import os
from os import listdir
from os.path import join
from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer

from minifi import SingleNodeDockerCluster

logging.basicConfig(level=logging.DEBUG)

def put_file_contents(contents, file_abs_path):
    logging.info('Writing %d bytes of content to file: %s', len(contents), file_abs_path)
    with open(file_abs_path, 'wb') as test_input_file:
        test_input_file.write(contents)


class DockerTestCluster(SingleNodeDockerCluster):
    def __init__(self, output_validator):

        # Create test input/output directories
        test_cluster_id = str(uuid.uuid4())

        self.segfault = False

        self.tmp_test_output_dir = '/tmp/.nifi-test-output.' + test_cluster_id
        self.tmp_test_input_dir = '/tmp/.nifi-test-input.' + test_cluster_id
        self.tmp_test_resources_dir = '/tmp/.nifi-test-resources.' + test_cluster_id

        logging.info('Creating tmp test input dir: %s', self.tmp_test_input_dir)
        os.makedirs(self.tmp_test_input_dir)
        logging.info('Creating tmp test output dir: %s', self.tmp_test_output_dir)
        os.makedirs(self.tmp_test_output_dir)
        logging.info('Creating tmp test resource dir: %s', self.tmp_test_resources_dir)
        os.makedirs(self.tmp_test_resources_dir)

        # Point output validator to ephemeral output dir
        self.output_validator = output_validator
        if isinstance(output_validator, SingleFileOutputValidator):
            output_validator.set_output_dir(self.tmp_test_output_dir)

        # Start observing output dir
        self.done_event = Event()
        event_handler = OutputEventHandler(output_validator, self.done_event)
        self.observer = Observer()
        self.observer.schedule(event_handler, self.tmp_test_output_dir)
        self.observer.start()

        super(DockerTestCluster, self).__init__()

    def deploy_flow(self,
                    flow,
                    name=None,
                    vols=None,
                    engine='minifi-cpp'):
        """
        Performs a standard container flow deployment with the addition
        of volumes supporting test input/output directories.
        """

        if vols is None:
            vols = {}

        vols[self.tmp_test_input_dir] = {'bind': '/tmp/input', 'mode': 'rw'}
        vols[self.tmp_test_output_dir] = {'bind': '/tmp/output', 'mode': 'rw'}
        vols[self.tmp_test_resources_dir] = {'bind': '/tmp/resources', 'mode': 'rw'}

        super(DockerTestCluster, self).deploy_flow(flow,
                                                   vols=vols,
                                                   name=name,
                                                   engine=engine)

    def put_test_data(self, contents):
        """
        Creates a randomly-named file in the test input dir and writes
        the given content to it.
        """

        file_name = str(uuid.uuid4())
        file_abs_path = join(self.tmp_test_input_dir, file_name)
        put_file_contents(contents.encode('utf-8'), file_abs_path)

    def put_test_resource(self, file_name, contents):
        """
        Creates a resource file in the test resource dir and writes
        the given content to it.
        """

        file_abs_path = join(self.tmp_test_resources_dir, file_name)
        put_file_contents(contents, file_abs_path)

    def wait_for_output(self, timeout_seconds):
        logging.info('Waiting up to %d seconds for test output...', timeout_seconds)
        self.done_event.wait(timeout_seconds)
        self.observer.stop()
        self.observer.join()

    def log_nifi_output(self):

        for container in self.containers:
            container = self.client.containers.get(container.id)
            logging.info('Container logs for container \'%s\':\n%s', container.name, container.logs())
            if b'Segmentation fault' in container.logs():
                self.segfault=True
            if container.status == 'running':
                minifi_app_logs = container.exec_run('/bin/sh -c \'test -f ' + self.minifi_root + '/logs/minifi-app.log '
                                                                                                '&& cat ' +
                                                     self.minifi_root + '/logs/minifi-app.log\'')
                if len(minifi_app_logs) > 0:
                    logging.info('MiNiFi app logs for container \'%s\':\n%s', container.name, minifi_app_logs)

                nifi_app_logs = container.exec_run('/bin/sh -c \'test -f ' + self.nifi_root + '/logs/nifi-app.log '
                                                                                            '&& cat ' +
                                                   self.nifi_root + '/logs/nifi-app.log\'')
                if len(nifi_app_logs) > 0:
                    logging.info('NiFi app logs for container \'%s\':\n%s', container.name, nifi_app_logs)
            else:
                logging.info(container.status)
                logging.info('Could not cat app logs for container \'%s\' because it is not running',
                             container.name)
            stats = container.stats(decode=True, stream=True)
            logging.info('Container stats:\n%s', repr(stats))

    def check_output(self, timeout=5):
        """
        Wait for flow output, validate it, and log minifi output.
        """
        self.wait_for_output(timeout)
        self.log_nifi_output()

        return self.output_validator.validate() and not self.segfault

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Clean up ephemeral test resources.
        """

        logging.info('Removing tmp test input dir: %s', self.tmp_test_input_dir)
        shutil.rmtree(self.tmp_test_input_dir)
        logging.info('Removing tmp test output dir: %s', self.tmp_test_output_dir)
        shutil.rmtree(self.tmp_test_output_dir)
        logging.info('Removing tmp test resources dir: %s', self.tmp_test_output_dir)
        shutil.rmtree(self.tmp_test_resources_dir)

        super(DockerTestCluster, self).__exit__(exc_type, exc_val, exc_tb)


class OutputEventHandler(FileSystemEventHandler):
    def __init__(self, validator, done_event):
        self.validator = validator
        self.done_event = done_event

    def on_created(self, event):
        logging.info('Output file created: ' + event.src_path)
        self.check(event)

    def on_modified(self, event):
        logging.info('Output file modified: ' + event.src_path)
        self.check(event)

    def check(self, event):
        if self.validator.validate():
            logging.info('Output file is valid')
            self.done_event.set()
        else:
            logging.info('Output file is invalid')


class OutputValidator(object):
    """
    Base output validator class. Validators must implement
    method validate, which returns a boolean.
    """

    def validate(self):
        """
        Return True if output is valid; False otherwise.
        """


class SingleFileOutputValidator(OutputValidator):
    """
    Validates the content of a single file in the given directory.
    """

    def __init__(self, expected_content):
        self.valid = False
        self.expected_content = expected_content

    def set_output_dir(self, output_dir):
        self.output_dir = output_dir

    def validate(self):

        if self.valid:
            return True

        listing = listdir(self.output_dir)

        if len(listing) > 0:
            out_file_name = listing[0]

            with open(join(self.output_dir, out_file_name), 'r') as out_file:
                contents = out_file.read()

                if contents == self.expected_content:
                    self.valid = True
                    return True

        return False

class SegfaultValidator(OutputValidator):
    """
    Validate that a file was received.
    """
    def validate(self):
        return True
