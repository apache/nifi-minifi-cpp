from ..core.Processor import Processor


class PutGCSObject(Processor):
    def __init__(
            self):
        super(PutGCSObject, self).__init__(
            'PutGCSObject',
            properties={
                'Bucket': 'test-bucket',
                'Endpoint Override URL': 'fake-gcs-server:4443',
                'Number of retries': 2
            },
            auto_terminate=["success", "failure"])
