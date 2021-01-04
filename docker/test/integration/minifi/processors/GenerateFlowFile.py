from ..core.Processor import Processor

class GenerateFlowFile(Processor):
    def __init__(self, file_size, schedule={'scheduling period': '0 sec'}):
        super(GenerateFlowFile, self).__init__('GenerateFlowFile',
			properties={'File Size': file_size},
			schedule=schedule,
			auto_terminate=['success'])
