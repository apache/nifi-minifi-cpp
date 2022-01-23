from ..core.Processor import Processor


class ListGCSBucket(Processor):
    def __init__(
            self):
        super(ListGCSBucket, self).__init__(
            'ListGCSBucket',
            properties={
                'Bucket': 'test-bucket',
                'Endpoint Override URL': 'fake-gcs-server:4443',
                'Number of retries': 2
            },
            auto_terminate=["success"])
