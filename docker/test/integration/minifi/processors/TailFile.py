from ..core.Processor import Processor


class TailFile(Processor):
    def __init__(self, filename="/tmp/input/test_file.log"):
        super(TailFile, self).__init__('TailFile',
                                       properties={'File to Tail': filename},
                                       auto_terminate=['success'])
