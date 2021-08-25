from ..core.Processor import Processor


class ExecutePythonProcessor(Processor):
    def __init__(self):
        super(ExecutePythonProcessor, self).__init__(
            'ExecutePythonProcessor',
            auto_terminate=['success'])
