from ..core.Processor import Processor


class ListenSyslog(Processor):
    def __init__(self, schedule=None):
        properties = {}

        super(ListenSyslog, self).__init__(
            'ListenSyslog',
            properties=properties,
            auto_terminate=['success'],
            schedule=schedule)
