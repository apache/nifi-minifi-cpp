from ..core.Processor import Processor


class FetchAzureBlobStorage(Processor):
    def __init__(self):
        super(FetchAzureBlobStorage, self).__init__(
            'FetchAzureBlobStorage',
            properties={
                'Container Name': 'test-container',
                'Connection String': 'DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint=http://azure-storage-server:10000/devstoreaccount1;QueueEndpoint=http://azure-storage-server:10001/devstoreaccount1;',
                'Blob': 'test-blob',
            },
            auto_terminate=['success', 'failure'])
