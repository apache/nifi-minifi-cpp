import docker
import logging
import os
import shutil
import threading
import time
import uuid
import datetime

from pydoc import locate

from minifi.core.InputPort import InputPort

from minifi.core.DockerTestCluster import DockerTestCluster
from minifi.core.SingleNodeDockerCluster import SingleNodeDockerCluster
from minifi.core.DockerTestDirectoryBindings import DockerTestDirectoryBindings

from minifi.validators.EmptyFilesOutPutValidator import EmptyFilesOutPutValidator
from minifi.validators.NoFileOutPutValidator import NoFileOutPutValidator
from minifi.validators.SingleFileOutputValidator import SingleFileOutputValidator
from minifi.validators.MultiFileOutputValidator import MultiFileOutputValidator
from minifi.validators.SingleOrMoreFileOutputValidator import SingleOrMoreFileOutputValidator
from minifi.validators.NoContentCheckFileNumberValidator import NoContentCheckFileNumberValidator
from minifi.validators.NumFileRangeValidator import NumFileRangeValidator


class MiNiFi_integration_test():
    def __init__(self, context):
        self.test_id = str(uuid.uuid4())
        self.clusters = {}

        self.connectable_nodes = []
        # Remote process groups are not connectables
        self.remote_process_groups = []
        self.file_system_observer = None

        self.docker_network = None
        self.cleanup_lock = threading.Lock()

        self.docker_directory_bindings = DockerTestDirectoryBindings()
        self.docker_directory_bindings.create_new_data_directories(self.test_id)

    def __del__(self):
        self.cleanup()

    def cleanup(self):
        with self.cleanup_lock:
            logging.info("MiNiFi_integration_test cleanup")
            # Clean up network, for some reason only this order of events work for cleanup
            if self.docker_network is not None:
                logging.info('Cleaning up network network: %s', self.docker_network.name)
                while len(self.docker_network.containers) != 0:
                    for container in self.docker_network.containers:
                        self.docker_network.disconnect(container, force=True)
                    self.docker_network.reload()
                self.docker_network.remove()
                self.docker_network = None

            container_ids = []
            for cluster in self.clusters.values():
                for container in cluster.containers.values():
                    container_ids.append(container.id)
                del cluster

            # The cluster deleter is not reliable for cleaning up
            logging.info("%d containers left for integration tests.", len(container_ids))
            docker_client = docker.from_env()
            for container_id in container_ids:
                self.delete_docker_container_by_id(container_id)

            if self.docker_directory_bindings is not None:
                del self.docker_directory_bindings
                self.docker_directory_bindings = None

    def delete_docker_container_by_id(self, container_id):
        docker_client = docker.from_env()
        try:
            container = docker_client.containers.get(container_id)
            container.remove(v=True, force=True)
        except docker.errors.NotFound:
            logging.warn("Contaner '%s' is already cleaned up before.", container_id)
            return
        wait_start_time = time.perf_counter()
        while (time.perf_counter() - wait_start_time) < 35:
            try:
                docker_client.containers.get(container_id)
                logging.error("Docker container '%s' still exists after removal attempt. Waiting for docker daemon to update...", container_id)
                time.sleep(5)
            except docker.errors.NotFound:
                logging.info("Docker container cleanup successful for '%s'.", container_id)
                return
        logging.error("Failed to clean up docker container '%s'.", container_id)

    def docker_path_to_local_path(self, docker_path):
        return self.docker_directory_bindings.docker_path_to_local_path(self.test_id, docker_path)

    def get_test_id(self):
        return self.test_id

    def acquire_cluster(self, name):
        return self.clusters.setdefault(name, DockerTestCluster())

    def set_up_cluster_network(self):
        if self.docker_network is None:
            logging.info("Setting up new network.")
            self.docker_network = SingleNodeDockerCluster.create_docker_network()
            for cluster in self.clusters.values():
                cluster.set_network(self.docker_network)
        else:
            logging.info("Network is already set.")

    def wait_for_cluster_startup_finish(self, cluster):
        startup_success = True
        logging.info("Engine: %s", cluster.get_engine())
        if cluster.get_engine() == "minifi-cpp":
            startup_success = cluster.wait_for_app_logs("Starting Flow Controller", 120)
        elif cluster.get_engine() == "nifi":
            startup_success = cluster.wait_for_app_logs("Starting Flow Controller...", 120)
        elif cluster.get_engine() == "kafka-broker":
            startup_success = cluster.wait_for_app_logs("Startup complete.", 120)
        elif cluster.get_engine() == "http-proxy":
            startup_success = cluster.wait_for_app_logs("Accepting HTTP Socket connections at", 120)
        elif cluster.get_engine() == "s3-server":
            startup_success = cluster.wait_for_app_logs("Started S3MockApplication", 120)
        elif cluster.get_engine() == "azure-storage-server":
            startup_success = cluster.wait_for_app_logs("Azurite Queue service is successfully listening at", 120)
        if not startup_success:
            cluster.log_nifi_output()
        return startup_success

    def start_single_cluster(self, cluster_name):
        self.set_up_cluster_network()
        cluster = self.clusters[cluster_name]
        cluster.deploy_flow()
        assert self.wait_for_cluster_startup_finish(cluster)

    def start(self):
        logging.info("MiNiFi_integration_test start")
        self.set_up_cluster_network()
        for cluster in self.clusters.values():
            if len(cluster.containers) == 0:
                logging.info("Starting cluster %s with an engine of %s", cluster.get_name(), cluster.get_engine())
                cluster.set_directory_bindings(self.docker_directory_bindings.get_directory_bindings(self.test_id))
                cluster.deploy_flow()
            else:
                logging.info("Container %s is already started with an engine of %s", cluster.get_name(), cluster.get_engine())
        for cluster in self.clusters.values():
            assert self.wait_for_cluster_startup_finish(cluster)
        # Seems like some extra time needed for consumers to negotiate with the broker
        for cluster in self.clusters.values():
            if cluster.get_engine() == "kafka-broker":
                time.sleep(10)

    def add_node(self, processor):
        if processor.get_name() in (elem.get_name() for elem in self.connectable_nodes):
            raise Exception("Trying to register processor with an already registered name: \"%s\"" % processor.get_name())
        self.connectable_nodes.append(processor)

    def get_or_create_node_by_name(self, node_name):
        node = self.get_node_by_name(node_name)
        if node is None:
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

    def add_test_data(self, path, test_data, file_name=str(uuid.uuid4())):
        self.docker_directory_bindings.put_file_to_docker_path(self.test_id, path, file_name, test_data.encode('utf-8'))

    def put_test_resource(self, file_name, contents):
        self.docker_directory_bindings.put_test_resource(self.test_id, file_name, contents)

    def get_out_subdir(self, subdir):
        return self.docker_directory_bindings.get_out_subdir(self.test_id, subdir)

    def rm_out_child(self, subdir):
        self.docker_directory_bindings.rm_out_child(self.test_id, subdir)

    def add_file_system_observer(self, file_system_observer):
        self.file_system_observer = file_system_observer

    def check_for_no_files_generated(self, timeout_seconds, subdir=''):
        output_validator = NoFileOutPutValidator()
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.check_output(timeout_seconds, output_validator, 1, subdir)

    def check_for_single_file_with_content_generated(self, content, timeout_seconds, subdir=''):
        output_validator = SingleFileOutputValidator(content)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.check_output(timeout_seconds, output_validator, 1, subdir)

    def check_for_multiple_files_generated(self, file_count, timeout_seconds, subdir=''):
        output_validator = MultiFileOutputValidator(file_count, subdir)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.check_output(timeout_seconds, output_validator, file_count, subdir)
    def check_for_at_least_one_file_with_content_generated(self, content, timeout_seconds, subdir=''):
        output_validator = SingleOrMoreFileOutputValidator(content)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.check_output(timeout_seconds, output_validator, 1, subdir)

    def check_for_num_files_generated(self, num_flowfiles, timeout_seconds, subdir=''):
        output_validator = NoContentCheckFileNumberValidator(num_flowfiles)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.check_output(timeout_seconds, output_validator, max(1, num_flowfiles), subdir)

    def check_for_num_file_range_generated(self, min_files, max_files, timeout_seconds, subdir=''):
        output_validator = NumFileRangeValidator(min_files, max_files)
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.check_output_force_wait(timeout_seconds, output_validator, subdir)

    def check_for_multiple_empty_files_generated(self, timeout_seconds, subdir=''):
        output_validator = EmptyFilesOutPutValidator()
        output_validator.set_output_dir(self.file_system_observer.get_output_dir())
        self.check_output(timeout_seconds, output_validator, 2, subdir)

    def wait_for_multiple_output_files(self, timeout_seconds, max_files):
        self.file_system_observer.wait_for_output(timeout_seconds, max_files)

    def check_output_force_wait(self, timeout_seconds, output_validator, subdir):
        if subdir:
            output_validator.subdir = subdir
        time.sleep(timeout_seconds)
        self.validate(output_validator)

    def check_output(self, timeout_seconds, output_validator, max_files, subdir):
        if subdir:
            output_validator.subdir = subdir
        # Other interfaces only call this with a single file,
        # call wait_for_multiple_output_files manually if multiple
        # output files with different content are expected in the same directory
        self.file_system_observer.wait_for_output(timeout_seconds, max_files)
        self.validate(output_validator)

    def validate(self, validator):
        for cluster in self.clusters.values():
            # Logs for both nifi and minifi, but not other engines
            if cluster.get_engine() != "kafka-broker":
                cluster.log_nifi_output()
            # cluster.log_nifi_output()
            assert not cluster.segfault_happened()
        assert validator.validate()

    def check_s3_server_object_data(self, cluster_name, object_data):
        cluster = self.acquire_cluster(cluster_name)
        assert cluster.check_s3_server_object_data(object_data)

    def check_s3_server_object_metadata(self, cluster_name, content_type):
        cluster = self.acquire_cluster(cluster_name)
        assert cluster.check_s3_server_object_metadata(content_type)

    def check_empty_s3_bucket(self, cluster_name):
        cluster = self.acquire_cluster(cluster_name)
        assert cluster.is_s3_bucket_empty()

    def check_http_proxy_access(self, cluster_name, url):
        assert self.clusters[cluster_name].check_http_proxy_access(url)

    def check_azure_storage_server_data(self, cluster_name, object_data):
        cluster = self.acquire_cluster(cluster_name)
        assert cluster.check_azure_storage_server_data(object_data)
