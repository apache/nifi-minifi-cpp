import docker
import tempfile
import os
import logging
from typing import Optional
from docker.models.images import Image


class DockerContainerBuilder:
    def __init__(self, image_tag: str, dockerfile_content: Optional[str] = None,
                 build_context_path: Optional[str] = None):
        if not dockerfile_content and not build_context_path:
            raise ValueError("Either 'dockerfile_content' or 'build_context_path' must be provided.")
        if dockerfile_content and build_context_path:
            raise ValueError("Provide either 'dockerfile_content' or 'build_context_path', not both.")

        self.image_tag: str = image_tag
        self.dockerfile_content: Optional[str] = dockerfile_content
        self.build_context_path: Optional[str] = build_context_path
        self.client = docker.from_env()
        self.image: Optional[Image] = None
        self._temp_dir: Optional[tempfile.TemporaryDirectory] = None

    def build(self) -> Image:
        context_path = self.build_context_path
        if self.dockerfile_content:
            self._temp_dir = tempfile.TemporaryDirectory()
            context_path = self._temp_dir.name
            dockerfile_path = os.path.join(context_path, 'Dockerfile')
            with open(dockerfile_path, 'w') as f:
                f.write(self.dockerfile_content)

        logging.info(f"Building Docker image '{self.image_tag}' from context '{context_path}'...")
        try:
            self.image, build_logs = self.client.images.build(
                path=context_path,
                tag=self.image_tag,
                rm=True,  # Remove intermediate containers
                forcerm=True  # Always remove intermediate containers
            )
            for log_line in build_logs:
                logging.debug(log_line.get('stream', '').strip())
            logging.info(f"Successfully built image '{self.image_tag}' (ID: {self.image.short_id})")
            return self.image
        except docker.errors.BuildError as e:
            logging.error(f"Failed to build image '{self.image_tag}'. Build logs:")
            for log_line in e.build_log:
                logging.error(log_line.get('stream', '').strip())
            raise
        finally:
            if self._temp_dir:
                self._temp_dir.cleanup()

    def remove_image(self):
        if not self.image:
            logging.warning(f"No image object to remove for tag '{self.image_tag}'. Trying to find by tag.")
            try:
                self.image = self.client.images.get(self.image_tag)
            except docker.errors.ImageNotFound:
                logging.info(f"Image '{self.image_tag}' not found, cleanup not needed.")
                return

        logging.info(f"Removing dynamically built image '{self.image_tag}'...")
        try:
            self.client.images.remove(image=self.image.id, force=True)
            logging.info("Image removed successfully.")
        except docker.errors.ImageNotFound:
            logging.info("Image was already removed.")
        except docker.errors.APIError as e:
            logging.error(f"Error removing image: {e}")
