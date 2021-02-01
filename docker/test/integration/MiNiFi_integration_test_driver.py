from subprocess import Popen, PIPE, STDOUT

import logging
import os
import shutil
import threading
import time
import uuid

from pydoc import locate

from minifi.core.InputPort import InputPort

from minifi.core.DockerTestCluster import DockerTestCluster
from minifi.core.SingleNodeDockerCluster import SingleNodeDockerCluster
from minifi.core.DockerTestDirectoryBindings import DockerTestDirectoryBindings

class MiNiFi_integration_test():
    def __init__(self, context):
        logging.info("MiNiFi_integration_test init")
        self.test_id = str(uuid.uuid4())
        self.clusters = {}

        self.connectable_nodes = []
        # Remote process groups are not connectables
        self.remote_process_groups = []
        # self.filesystem_observers = []
        # self.validators = []
        self.file_system_observer = None
        self.validator = None

        self.docker_network = None

        self.docker_directory_bindings = DockerTestDirectoryBindings()
        self.docker_directory_bindings.create_new_data_directories(self.test_id)

    def __del__(self):
        logging.info("MiNiFi_integration_test cleanup")

        # Clean up network
        if self.docker_network is not None:
            logging.info('Cleaning up network network: %s', self.docker_network.name)
            while len(self.docker_network.containers) != 0:
                for container in self.docker_network.containers:
                    self.docker_network.disconnect(container, force=True)
                self.docker_network.reload()
            self.docker_network.remove()

        for cluster in self.clusters:
            del cluster

        del self.docker_directory_bindings

    def docker_path_to_local_path(self, docker_path):
        return self.docker_directory_bindings.docker_path_to_local_path(self.test_id, docker_path)

    def get_test_id(self):
        return self.test_id

    def acquire_cluster(self, name):
        return self.clusters.setdefault(name, DockerTestCluster())

    def set_up_cluster_network(self):
        self.docker_network = SingleNodeDockerCluster.create_docker_network()
        for cluster in self.clusters.values():
            cluster.set_network(self.docker_network)

    def start(self):
        logging.info("MiNiFi_integration_test start")
        self.set_up_cluster_network()
        # for node in self.connectable_nodes:
        #     logging.info("Node name: %s", node.get_name())
        #     if isinstance(node, Processor):
        #         for property_name, property_value in node.properties.items():
        #             logging.info("Property %s is set to %s", property_name, property_value)
        for cluster in self.clusters.values():
            cluster.set_directory_bindings(self.docker_directory_bindings.get_directory_bindings(self.test_id))
            cluster.deploy_flow()

    def add_node(self, processor):
        if processor.get_name() in (elem.get_name() for elem in self.connectable_nodes):
            raise Exception("Trying to register processor with an already registered name: \"%s\"" % processor.get_name())
        self.connectable_nodes.append(processor)

    def get_or_create_node_by_name(self, node_name):
        node = self.get_node_by_name(node_name) 
        if node == None:
            if node_name == "RemoteProcessGroup":
                raise Exception("Trying to register RemoteProcessGroup without an input port or address.")
            node = locate("minifi.processors." + node_name + "." + node_name)()
            node.set_name(node_name)
            self.add_node(node)
        return node

    def get_node_by_name(self, name):
        for node in self.connectable_nodes:
            if name == node.get_name():
                return node
        raise Exception("Trying to fetch unknow node: \"%s\"" % name)

    def add_remote_process_group(self, remote_process_group):
        if remote_process_group.get_name() in (elem.get_name() for elem in self.remote_process_groups):
            raise Exception("Trying to register remote_process_group with an already registered name: \"%s\"" % remote_process_group.get_name())
        self.remote_process_groups.append(remote_process_group)

    def get_remote_process_group_by_name(self, name):
        for node in self.remote_process_groups:
            if name == node.get_name():
                return node
        raise Exception("Trying to fetch unknow node: \"%s\"" % name)

    @staticmethod
    def generate_input_port_for_remote_process_group(remote_process_group, name):
        input_port_node = InputPort(name, remote_process_group)
        # Generate an MD5 hash unique to the remote process group id
        input_port_node.set_uuid(uuid.uuid3(remote_process_group.get_uuid(), "input_port"))
        return input_port_node

    def add_test_data(self, test_data):
        file_name = str(uuid.uuid4())
        self.docker_directory_bindings.put_test_input(self.test_id, file_name, test_data.encode('utf-8'))

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
        for cluster in self.clusters.values():
            cluster.log_nifi_output()
            assert not cluster.segfault_happened()
        assert self.file_system_observer.validate_output()
