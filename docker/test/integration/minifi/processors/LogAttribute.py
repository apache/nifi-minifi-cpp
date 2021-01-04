from ..core.Processor import Processor

class LogAttribute(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(LogAttribute, self).__init__('LogAttribute',
			auto_terminate=['success'],
			schedule=schedule)
