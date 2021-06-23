import docker
import logging
import tarfile
from io import BytesIO


class Container:
    def __init__(self, name, engine, vols, network):
        self.name = name
        self.engine = engine
        self.vols = vols
        self.network = network

        # Get docker client
        self.client = docker.from_env()
        self.docker_container = None
        self.image = None
        self.deployed = False

    def __del__(self):
        logging.info('Cleaning up container: %s', self.name)
        container = self.client.containers.get(self.name)
        if container:
            container.remove(v=True, force=True)

        # Clean up images
        if self.image:
            logging.info('Cleaning up image: %s', self.image[0].id)
            self.client.images.remove(self.image[0].id, force=True)

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

    def get_startup_finish_text(self):
        raise NotImplementedError()

    def get_log_file_path(self):
        return None

    def build_image(self, dockerfile, context_files):
        conf_dockerfile_buffer = BytesIO()
        docker_context_buffer = BytesIO()

        try:
            # Overlay conf onto base nifi image
            conf_dockerfile_buffer.write(dockerfile.encode())
            conf_dockerfile_buffer.seek(0)

            with tarfile.open(mode='w', fileobj=docker_context_buffer) as docker_context:
                dockerfile_info = tarfile.TarInfo('Dockerfile')
                dockerfile_info.size = conf_dockerfile_buffer.getbuffer().nbytes
                docker_context.addfile(dockerfile_info,
                                       fileobj=conf_dockerfile_buffer)

                for context_file in context_files:
                    file_info = tarfile.TarInfo(context_file['name'])
                    file_info.size = context_file['size']
                    docker_context.addfile(file_info,
                                           fileobj=context_file['file_obj'])
            docker_context_buffer.seek(0)

            logging.info('Creating configured image...')
            configured_image = self.client.images.build(fileobj=docker_context_buffer,
                                                        custom_context=True,
                                                        rm=True,
                                                        forcerm=True)
            logging.info('Created image with id: %s', configured_image[0].id)
            self.image = configured_image

        finally:
            conf_dockerfile_buffer.close()
            docker_context_buffer.close()

        return configured_image

    def build_image_by_path(self, dir, name=None):
        try:
            logging.info('Creating configured image...')
            configured_image = self.client.images.build(path=dir,
                                                        tag=name,
                                                        rm=True,
                                                        forcerm=True)
            logging.info('Created image with id: %s', configured_image[0].id)
            self.image = configured_image
            return configured_image
        except Exception as e:
            logging.info(e)
            raise
