from subprocess import Popen, PIPE, STDOUT

import logging
import os
import time

class MiNiFi_integration_test():
    def __init__(self, context):
        logging.info("MiNiFi_integration_test init")
        self.cluster = None
        self.processors = []
        self.connections = []
        self.validators = []

    def __del__(self):
        self.cluster = None
        self.stop()

    def set_cluster(self, cluster):
        self.cluster = cluster

    def start(self, flow):
        logging.info("MiNiFi_integration_test start")
        self.cluster.deploy_flow(flow)

    def add_processor(self, processor):
        self.processors.append(processor)

    def add_test_data(self, test_data):
        self.cluster.put_test_data(test_data)

    def add_validator(self, validator):
        self.validators.append(validator)

    def stop(self):
        logging.info("MiNiFi_integration_test start")
        # logging.info("Stopping daemon", end="\n\n")
        
    def is_running(self):
        logging.info("MiNiFi_integration_test is_running check")
        # minifi_status.wait()
        # response = str(minifi_status.stdout.read())
        # return True if "MINIFI is currently running" in response else False

    def check_output(self):
        assert self.cluster.check_output()
