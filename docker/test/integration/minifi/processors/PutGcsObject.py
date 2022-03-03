from ..core.Processor import Processor


class PutGcsObject(Processor):
    def __init__(
            self):
        super(PutGcsObject, self).__init__(
            'PutGcsObject',
            properties={
                'Bucket Name': 'test-bucket',
                'Endpoint Override URL': 'fake-gcs-server:4443',
                'Number of retries': 2
            },
            auto_terminate=["success", "failure"])
