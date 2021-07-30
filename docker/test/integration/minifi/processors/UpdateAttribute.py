from ..core.Processor import Processor


class UpdateAttribute(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(UpdateAttribute, self).__init__(
            'UpdateAttribute',
            auto_terminate=['success'],
            schedule=schedule)
