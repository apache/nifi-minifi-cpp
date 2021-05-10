from ..core.Processor import Processor


class ExecuteSQL(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(ExecuteSQL, self).__init__(
            'ExecuteSQL',
            auto_terminate=['success'],
            schedule=schedule)
