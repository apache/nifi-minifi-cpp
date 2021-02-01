import logging
import os
import shutil

class DockerTestDirectoryBindings:
    def __init__(self):
        self.data_directories = {}

    def __del__(self):
        self.delete_data_directories()

    def create_new_data_directories(self, test_id):
        self.data_directories[test_id] = {
            "input_dir": "/tmp/.nifi-test-input." + test_id,
            "output_dir": "/tmp/.nifi-test-output." + test_id,
            "resources_dir": "/tmp/.nifi-test-resources." + test_id
        }

        [self.create_directory(directory) for directory in self.data_directories[test_id].values()]

        # Add resources
        test_dir = os.environ['PYTHONPATH'].split(':')[-1] # Based on DockerVerify.sh
        shutil.copytree(test_dir + "/resources/kafka_broker/conf/certs", self.data_directories[test_id]["resources_dir"] + "/certs")

    def get_data_directories(self, test_id):
        return self.data_directories[test_id]

    def docker_path_to_local_path(self, test_id, docker_path):
        # Docker paths are currently hard-coded
        if docker_path == "/tmp/input":
            return self.data_directories[test_id]["input_dir"]
        if docker_path == "/tmp/output":
            return self.data_directories[test_id]["output_dir"]
        if docker_path == "/tmp/resources":
            return self.data_directories[test_id]["resources_dir"]
        raise Exception("Docker directory \"%s\" has no preset bindings." % docker_path)

    def get_directory_bindings(self, test_id):
        """
        Performs a standard container flow deployment with the addition
        of volumes supporting test input/output directories.
        """
        vols = {}
        vols[self.data_directories[test_id]["input_dir"]] = {"bind": "/tmp/input", "mode": "rw"}
        vols[self.data_directories[test_id]["output_dir"]] = {"bind": "/tmp/output", "mode": "rw"}
        vols[self.data_directories[test_id]["resources_dir"]] = {"bind": "/tmp/resources", "mode": "rw"}
        return vols

    @staticmethod
    def create_directory(dir):
        logging.info("Creating tmp dir: %s", dir)
        os.makedirs(dir)
        os.chmod(dir, 0o777)

    @staticmethod
    def delete_directory(dir):
        logging.info("Removing tmp dir: %s", dir)
        shutil.rmtree(dir)

    def delete_data_directories(self):
        for directories in self.data_directories.values():
            [self.delete_directory for directory in directories.values()]

    @staticmethod
    def put_file_contents(file_abs_path, contents):
        logging.info('Writing %d bytes of content to file: %s', len(contents), file_abs_path)
        with open(file_abs_path, 'wb') as test_input_file:
            test_input_file.write(contents)

    def put_test_resource(self, test_id, file_name, contents):
        """
        Creates a resource file in the test resource dir and writes
        the given content to it.
        """

        file_abs_path = os.path.join(self.data_directories[test_id]["resources_dir"], file_name)
        self.put_file_contents(file_abs_path, contents)

    def put_test_input(self, test_id, file_name, contents):
        file_abs_path = os.path.join(self.data_directories[test_id]["input_dir"], file_name)
        self.put_file_contents(file_abs_path, contents)
