from .AzureBlobStorageProcessorBase import AzureBlobStorageProcessorBase


class DeleteAzureBlobStorage(AzureBlobStorageProcessorBase):
    def __init__(self):
        super(DeleteAzureBlobStorage, self).__init__(
            'DeleteAzureBlobStorage',
            schedule={"scheduling strategy": "EVENT_DRIVEN"})
