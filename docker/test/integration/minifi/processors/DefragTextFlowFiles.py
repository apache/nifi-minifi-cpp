from ..core.Processor import Processor


class DefragTextFlowFiles(Processor):
    def __init__(self, delimiter="<[0-9]+>", schedule={'scheduling period': '2 sec'}):
        super(DefragTextFlowFiles, self).__init__('DefragTextFlowFiles',
                                                  schedule=schedule,
                                                  properties={'Delimiter': delimiter},
                                                  auto_terminate=['success', 'failure'])
