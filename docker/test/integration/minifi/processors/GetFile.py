from ..core.Processor import Processor


class GetFile(Processor):
    def __init__(self, input_dir="/tmp/input", schedule={'scheduling period': '2 sec'}):
        super(GetFile, self).__init__(
            'GetFile',
            properties={
                'Input Directory': input_dir,
                'Keep Source File': 'true'
            },
            schedule=schedule,
            auto_terminate=['success'])
