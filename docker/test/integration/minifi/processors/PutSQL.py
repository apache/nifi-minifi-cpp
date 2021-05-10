from ..core.Processor import Processor


class PutSQL(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(PutSQL, self).__init__(
            'PutSQL',
            auto_terminate=['success'],
            schedule=schedule)
