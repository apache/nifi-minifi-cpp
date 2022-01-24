from .AzureBlobStorageProcessorBase import AzureBlobStorageProcessorBase


class PutAzureBlobStorage(AzureBlobStorageProcessorBase):
    def __init__(self):
        super(PutAzureBlobStorage, self).__init__(
            'PutAzureBlobStorage',
            {'Create Container': 'true'})
