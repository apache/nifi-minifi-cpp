from ..core.Processor import Processor


class AttributesToJSON(Processor):
    def __init__(self):
        super(AttributesToJSON, self).__init__('AttributesToJSON', auto_terminate=['failure'])
