from ..core.Processor import Processor


class DefragmentText(Processor):
    def __init__(self, delimiter="<[0-9]+>", schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(DefragmentText, self).__init__('DefragmentText',
                                             schedule=schedule,
                                             properties={'Delimiter': delimiter},
                                             auto_terminate=['success', 'failure'])
