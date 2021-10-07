from ..core.Processor import Processor


class DeleteAzureBlobStorage(Processor):
    def __init__(self):
        super(DeleteAzureBlobStorage, self).__init__(
            'DeleteAzureBlobStorage',
            properties={
                'Container Name': 'test-container',
                'Connection String': 'DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint=http://azure-storage-server:10000/devstoreaccount1;QueueEndpoint=http://azure-storage-server:10001/devstoreaccount1;',
                'Blob': 'test-blob',
                'Create Container': 'true',
            },
            auto_terminate=['success', 'failure'])
