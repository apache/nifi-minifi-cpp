from ..core.Processor import Processor


class ExecuteScript(Processor):
    def __init__(self):
        super(ExecuteScript, self).__init__(
            'ExecuteScript',
            auto_terminate=['success', 'failure'])
