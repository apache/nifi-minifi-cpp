from ..core.Processor import Processor

class GenerateFlowFile(Processor):
    def __init__(self, schedule={'scheduling period': '2 sec'}):
        super(GenerateFlowFile, self).__init__('GenerateFlowFile',
			schedule=schedule,
			auto_terminate=['success'])
