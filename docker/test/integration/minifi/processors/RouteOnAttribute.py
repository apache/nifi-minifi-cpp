from ..core.Processor import Processor


class RouteOnAttribute(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(RouteOnAttribute, self).__init__(
            'RouteOnAttribute',
            properties={},
            schedule=schedule,
            auto_terminate=['unmatched', "failure"])
