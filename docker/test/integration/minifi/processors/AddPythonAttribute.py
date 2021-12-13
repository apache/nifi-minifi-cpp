from ..core.Processor import Processor


class AddPythonAttribute(Processor):
    def __init__(self):
        super(AddPythonAttribute, self).__init__('AddPythonAttribute', class_prefix='org.apache.nifi.minifi.processors.examples.')
