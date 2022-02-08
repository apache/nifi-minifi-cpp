import docker
import logging


class Container:
    def __init__(self, name, engine, vols, network, image_store, command):
        self.name = name
        self.engine = engine
        self.vols = vols
        self.network = network
        self.image_store = image_store
        self.command = command

        # Get docker client
        self.client = docker.from_env()
        self.deployed = False

    def __del__(self):
        self.cleanup()

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

    def type(self):
        return 'docker container'

    def get_startup_finished_log_entry(self):
        raise NotImplementedError()

    def get_log_file_path(self):
        return None
