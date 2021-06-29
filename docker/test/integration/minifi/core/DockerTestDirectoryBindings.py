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
        test_dir = os.environ['PYTHONPATH'].split(':')[-1]  # Based on DockerVerify.sh
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
        # Might be worth reworking these
        if docker_path == "/tmp/output/success":
            self.create_directory(self.data_directories[test_id]["output_dir"] + "/success")
            return self.data_directories[test_id]["output_dir"] + "/success"
        if docker_path == "/tmp/output/failure":
            self.create_directory(self.data_directories[test_id]["output_dir"] + "/failure")
            return self.data_directories[test_id]["output_dir"] + "/failure"
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
        os.makedirs(dir)
        os.chmod(dir, 0o777)

    @staticmethod
    def delete_tmp_directory(dir):
        assert dir.startswith("/tmp/")
        if not dir.endswith("/"):
            dir = dir + "/"
        # Sometimes rmtree does clean not up as expected, setting ignore_errors does not help either
        shutil.rmtree(dir, ignore_errors=True)

    def delete_data_directories(self):
        for directories in self.data_directories.values():
            for directory in directories.values():
                self.delete_tmp_directory(directory)

    @staticmethod
    def put_file_contents(file_abs_path, contents):
        logging.info('Writing %d bytes of content to file: %s', len(contents), file_abs_path)
        with open(file_abs_path, 'ab') as test_input_file:
            test_input_file.write(contents)
        os.chmod(file_abs_path, 0o0777)

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

    def put_file_to_docker_path(self, test_id, path, file_name, contents):
        file_abs_path = os.path.join(self.docker_path_to_local_path(test_id, path), file_name)
        self.put_file_contents(file_abs_path, contents)

    def rm_out_child(self, test_id, dir):
        child = os.path.join(self.data_directories[test_id]["output_dir"], dir)
        logging.info('Removing %s from output folder', child)
        shutil.rmtree(child)
