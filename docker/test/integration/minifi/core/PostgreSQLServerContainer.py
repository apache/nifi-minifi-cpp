from .Container import Container


class PostgreSQLServerContainer(Container):
    def __init__(self, name, vols, network, image_store, command=None):
        super().__init__(name, 'postgresql-server', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "database system is ready to accept connections"

    def deploy(self):
        if not self.set_deployed():
            return

        self.docker_container = self.client.containers.run(
            self.image_store.get_image(self.get_engine()),
            detach=True,
            name='postgresql-server',
            network=self.network.name,
            environment=["POSTGRES_PASSWORD=password"],
            entrypoint=self.command)
