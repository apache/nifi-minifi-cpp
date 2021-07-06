from ..core.Processor import Processor


class ExecutePythonProcessor(Processor):
    def __init__(self):
        super(ExecutePythonProcessor, self).__init__(
            'ExecutePythonProcessor',
            properties={'Script File': "/tmp/resources/python/add_attribute_to_flowfile.py"},
            auto_terminate=['success'])
