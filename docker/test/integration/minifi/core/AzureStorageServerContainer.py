from .Container import Container


class AzureStorageServerContainer(Container):
    def __init__(self, name, vols, network):
        super().__init__(name, 'azure-storage-server', vols, network)

    def get_startup_finish_text(self):
        return "Azurite Queue service is successfully listening at"

    def deploy(self):
        if not self.set_deployed():
            return

        self.client.containers.run(
            "mcr.microsoft.com/azure-storage/azurite:3.13.0",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'10000/tcp': 10000, '10001/tcp': 10001})
