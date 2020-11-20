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
import tarfile
import subprocess
import sys
import time
import subprocess
import json
from io import BytesIO
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
        os.chmod(self.tmp_test_output_dir, 0o777)
        os.chmod(self.tmp_test_input_dir, 0o777)
        os.chmod(self.tmp_test_resources_dir, 0o777)

        # Add resources
        test_dir = os.environ['PYTHONPATH'].split(':')[-1] # Based on DockerVerify.sh
        shutil.copytree(test_dir + "/resources/kafka_broker/conf/certs", self.tmp_test_resources_dir + "/certs")

        # Point output validator to ephemeral output dir
        self.output_validator = output_validator
        if isinstance(output_validator, FileOutputValidator):
            output_validator.set_output_dir(self.tmp_test_output_dir)

        # Start observing output dir
        self.done_event = Event()
        self.event_handler = OutputEventHandler(self.output_validator, self.done_event)
        self.observer = Observer()
        self.observer.schedule(self.event_handler, self.tmp_test_output_dir)
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

    def start_flow(self, name):
        container = self.containers[name]
        container.reload()
        logging.info("Status before start: %s", container.status)
        if container.status == 'exited':
            logging.info("Start container: %s", name)
            container.start()
            return True
        return False

    def stop_flow(self, name):
        container = self.containers[name]
        container.reload()
        logging.info("Status before stop: %s", container.status)
        if container.status == 'running':
            logging.info("Stop container: %s", name)
            container.stop(timeout=0)
            return True
        return False

    def put_test_data(self, contents):
        """
        Creates a randomly-named file in the test input dir and writes
        the given content to it.
        """

        self.test_data = contents
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

    def restart_observer_if_needed(self):
        if self.observer.is_alive():
            return

        self.observer = Observer()
        self.done_event.clear()
        self.observer.schedule(self.event_handler, self.tmp_test_output_dir)
        self.observer.start()

    def wait_for_output(self, timeout_seconds):
        logging.info('Waiting up to %d seconds for test output...', timeout_seconds)
        self.restart_observer_if_needed()
        self.done_event.wait(timeout_seconds)
        self.observer.stop()
        self.observer.join()

    def log_nifi_output(self):

        for container in self.containers.values():
            container = self.client.containers.get(container.id)
            logging.info('Container logs for container \'%s\':\n%s', container.name, container.logs().decode("utf-8"))
            if b'Segmentation fault' in container.logs():
                logging.warn('Container segfaulted: %s', container.name)
                self.segfault=True
            if container.status == 'running':
                apps = [("MiNiFi", self.minifi_root + '/logs/minifi-app.log'), ("NiFi", self.nifi_root + '/logs/nifi-app.log')]
                for app in apps:
                    app_log_status, app_log = container.exec_run('/bin/sh -c \'cat ' + app[1] + '\'')
                    if app_log_status == 0:
                        logging.info('%s app logs for container \'%s\':\n', app[0], container.name)
                        for line in app_log.decode("utf-8").splitlines():
                            logging.info(line)
                        break
                else:
                    logging.warning("The container is running, but none of %s logs were found", " or ".join([x[0] for x in apps]))

            else:
                logging.info(container.status)
                logging.info('Could not cat app logs for container \'%s\' because it is not running',
                             container.name)
            stats = container.stats(stream=False)
            logging.info('Container stats:\n%s', stats)

    def check_output(self, timeout=10, subdir=''):
        """
        Wait for flow output, validate it, and log minifi output.
        """
        if subdir:
            self.output_validator.subdir = subdir
        self.wait_for_output(timeout)
        self.log_nifi_output()
        if self.segfault:
            return False
        return self.output_validator.validate()

    def check_http_proxy_access(self, url):
        output = subprocess.check_output(["docker", "exec", "http-proxy", "cat", "/var/log/squid/access.log"]).decode(sys.stdout.encoding)
        return url in output and \
            ((output.count("TCP_DENIED/407") != 0 and \
              output.count("TCP_MISS/200") == output.count("TCP_DENIED/407")) or \
             output.count("TCP_DENIED/407") == 0 and "TCP_MISS/200" in output)

    def check_s3_server_object_data(self):
        s3_mock_dir = subprocess.check_output(["docker", "exec", "s3-server", "find", "/tmp/", "-type", "d", "-name", "s3mock*"]).decode(sys.stdout.encoding).strip()
        file_data = subprocess.check_output(["docker", "exec", "s3-server", "cat", s3_mock_dir + "/test_bucket/test_object_key/fileData"]).decode(sys.stdout.encoding)
        return file_data == self.test_data

    def check_s3_server_object_metadata(self, content_type="application/octet-stream", metadata=dict()):
        s3_mock_dir = subprocess.check_output(["docker", "exec", "s3-server", "find", "/tmp/", "-type", "d", "-name", "s3mock*"]).decode(sys.stdout.encoding).strip()
        metadata_json = subprocess.check_output(["docker", "exec", "s3-server", "cat", s3_mock_dir + "/test_bucket/test_object_key/metadata"]).decode(sys.stdout.encoding)
        server_metadata = json.loads(metadata_json)
        return server_metadata["contentType"] == content_type and metadata == server_metadata["userMetadata"]

    def rm_out_child(self, dir):
        logging.info('Removing %s from output folder', os.path.join(self.tmp_test_output_dir, dir))
        shutil.rmtree(os.path.join(self.tmp_test_output_dir, dir))

    def wait_for_container_logs(self, container_name, log, timeout, count=1):
        logging.info('Waiting for logs `%s` in container `%s`', log, container_name)
        container = self.containers[container_name]
        check_count = 0
        while check_count <= timeout:
            if container.logs().decode("utf-8").count(log) == count:
                return True
            else:
                check_count += 1
                time.sleep(1)
        return False

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
        raise NotImplementedError("validate function needs to be implemented for validators")



class FileOutputValidator(OutputValidator):
    def set_output_dir(self, output_dir):
        self.output_dir = output_dir

    def validate(self, dir=''):
        pass

class SingleFileOutputValidator(FileOutputValidator):
    """
    Validates the content of a single file in the given directory.
    """

    def __init__(self, expected_content, subdir=''):
        self.valid = False
        self.expected_content = expected_content
        self.subdir = subdir

    def validate(self):
        self.valid = False
        full_dir = os.path.join(self.output_dir, self.subdir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return self.valid

        listing = listdir(full_dir)
        if listing:
            for l in listing:
                logging.info("name:: %s", l)
            out_file_name = listing[0]
            full_path = join(full_dir, out_file_name)
            if not os.path.isfile(full_path):
                return self.valid

            with open(full_path, 'r') as out_file:
                contents = out_file.read()
                logging.info("dir %s -- name %s", full_dir, out_file_name)
                logging.info("expected %s -- content %s", self.expected_content, contents)

                if self.expected_content in contents:
                    self.valid = True

        return self.valid


class EmptyFilesOutPutValidator(FileOutputValidator):
    """
    Validates if all the files in the target directory are empty and at least one exists
    """
    def __init__(self):
        self.valid = False

    def validate(self, dir=''):

        if self.valid:
            return True

        full_dir = self.output_dir + dir
        logging.info("Output folder: %s", full_dir)
        listing = listdir(full_dir)
        if listing:
            self.valid = all(os.path.getsize(os.path.join(full_dir,x)) == 0 for x in listing)

        return self.valid

class NoFileOutPutValidator(FileOutputValidator):
    """
    Validates if no flowfiles were transferred
    """
    def __init__(self):
        self.valid = False

    def validate(self, dir=''):

        if self.valid:
            return True

        full_dir = self.output_dir + dir
        logging.info("Output folder: %s", full_dir)
        listing = listdir(full_dir)

        self.valid = not bool(listing)

        return self.valid


class SegfaultValidator(OutputValidator):
    """
    Validate that a file was received.
    """
    def validate(self):
        return True
