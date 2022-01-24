from .AzureBlobStorageProcessorBase import AzureBlobStorageProcessorBase


class FetchAzureBlobStorage(AzureBlobStorageProcessorBase):
    def __init__(self):
        super(FetchAzureBlobStorage, self).__init__(
            'FetchAzureBlobStorage',
            schedule={"scheduling strategy": "EVENT_DRIVEN"})
