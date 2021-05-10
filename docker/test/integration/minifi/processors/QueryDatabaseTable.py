from ..core.Processor import Processor


class QueryDatabaseTable(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(QueryDatabaseTable, self).__init__(
            'QueryDatabaseTable',
            auto_terminate=['success'],
            schedule=schedule)
