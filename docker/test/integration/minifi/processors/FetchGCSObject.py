from ..core.Processor import Processor


class FetchGCSObject(Processor):
    def __init__(
            self):
        super(FetchGCSObject, self).__init__(
            'FetchGCSObject',
            properties={
                'Bucket': 'test-bucket',
                'Endpoint Override URL': 'fake-gcs-server:4443',
                'Number of retries': 2
            },
            auto_terminate=["success", "failure"])
