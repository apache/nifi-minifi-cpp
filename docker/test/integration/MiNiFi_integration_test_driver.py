from subprocess import Popen, PIPE, STDOUT

import logging
import os
import shutil
import time
import uuid

from minifi.core.DockerTestDirectoryBindings import DockerTestDirectoryBindings

class MiNiFi_integration_test():
    def __init__(self, context):
        logging.info("MiNiFi_integration_test init")
        self.test_id = str(uuid.uuid4())
        self.cluster = None
        self.processors = []
        self.connections = []
        # self.filesystem_observers = []
        # self.validators = []
        self.file_system_observer = None
        self.validator = None

        self.tmp_test_output_dir = '/tmp/.nifi-test-output.' + self.test_id
        self.tmp_test_input_dir = '/tmp/.nifi-test-input.' + self.test_id
        self.tmp_test_resources_dir = '/tmp/.nifi-test-resources.' + self.test_id

        self.docker_directory_bindings = DockerTestDirectoryBindings()
        self.docker_directory_bindings.create_new_data_directories(self.test_id)

    def __del__(self):
        logging.info("MiNiFi_integration_test cleanup")

        self.docker_directory_bindings = None
        self.cluster = None

    def get_test_id(self):
        return self.test_id

    def set_cluster(self, cluster):
        self.cluster = cluster

    def start(self, flow):
        logging.info("MiNiFi_integration_test start")
        self.cluster.deploy_flow(flow, self.docker_directory_bindings.get_directory_bindings(self.test_id))

    def get_output_dir(self):
        return self.tmp_test_output_dir

    def add_processor(self, processor):
        if processor.get_name() in (elem.get_name() for elem in self.processors):
            raise Exception("Trying to register processor with an already registered name: \'%s\'" % processor.get_name())
        self.processors.append(processor)

    def get_processor_by_name(self, name):
        for processor in self.processors:
            if name == processor.get_name():
                return processor
        return None

    def add_test_data(self, test_data):
        file_name = str(uuid.uuid4())
        file_abs_path = os.path.join(self.tmp_test_input_dir, file_name)
        self.cluster.put_file_contents(test_data.encode('utf-8'), file_abs_path)

    def add_file_system_observer(self, file_system_observer):
        # self.filesystem_observers.append(file_system_observer)
        self.file_system_observer = file_system_observer

    def add_validator(self, validator):
        # self.validators.append(validator)
        self.validator = validator

    def get_validator(self):
        return self.validator

    def is_running(self):
        logging.info("MiNiFi_integration_test is_running check")
        # minifi_status.wait()
        # response = str(minifi_status.stdout.read())
        # return True if "MINIFI is currently running" in response else False

    def check_output(self, timeout_seconds, subdir=''):
        """
        Wait for flow output, validate it, and log minifi output.
        """
        if subdir:
            self.file_system_observer.set_output_validator_subdir(subdir)
        self.file_system_observer.wait_for_output(timeout_seconds)
        self.cluster.log_nifi_output()
        assert not self.cluster.segfault_happened()
        assert self.file_system_observer.validate_output()
