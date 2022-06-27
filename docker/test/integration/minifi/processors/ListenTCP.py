from ..core.Processor import Processor


class ListenTCP(Processor):
    def __init__(self, schedule=None):
        properties = {}

        super(ListenTCP, self).__init__(
            'ListenTCP',
            properties=properties,
            auto_terminate=['success'],
            schedule=schedule)
