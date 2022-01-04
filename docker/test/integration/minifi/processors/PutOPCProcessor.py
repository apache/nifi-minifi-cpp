from ..core.Processor import Processor


class PutOPCProcessor(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(PutOPCProcessor, self).__init__(
            'PutOPCProcessor',
            auto_terminate=['success', 'failure'],
            schedule=schedule)
