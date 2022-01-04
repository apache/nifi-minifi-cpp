from ..core.Processor import Processor


class FetchOPCProcessor(Processor):
    def __init__(self):
        super(FetchOPCProcessor, self).__init__(
            'FetchOPCProcessor',
            auto_terminate=['success', 'failure'])
