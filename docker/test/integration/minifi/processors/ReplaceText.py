from ..core.Processor import Processor


class ReplaceText(Processor):
    def __init__(self):
        super(ReplaceText, self).__init__(
            'ReplaceText',
            properties={},
            schedule={'scheduling strategy': 'EVENT_DRIVEN'},
            auto_terminate=['success', 'failure'])
