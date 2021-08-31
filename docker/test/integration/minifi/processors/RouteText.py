from ..core.Processor import Processor


class RouteText(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(RouteText, self).__init__(
            'RouteText',
            schedule=schedule,
            auto_terminate=['unmatched', "matched", "original"])
