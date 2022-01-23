from ..core.Processor import Processor


class DeleteGCSObject(Processor):
    def __init__(
            self):
        super(DeleteGCSObject, self).__init__(
            'DeleteGCSObject',
            properties={
                'Bucket': 'test-bucket',
                'Endpoint Override URL': 'fake-gcs-server:4443',
                'Number of retries': 2
            },
            auto_terminate=["success", "failure"])
