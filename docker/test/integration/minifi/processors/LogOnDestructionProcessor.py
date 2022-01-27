from ..core.Processor import Processor


class LogOnDestructionProcessor(Processor):
    def __init__(self, schedule={'scheduling period': '2 sec'}):
        super(LogOnDestructionProcessor, self).__init__(
            'LogOnDestructionProcessor',
            schedule=schedule)
