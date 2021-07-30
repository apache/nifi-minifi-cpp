from ..core.Processor import Processor


class QueryDatabaseTable(Processor):
    def __init__(self):
        super(QueryDatabaseTable, self).__init__(
            'QueryDatabaseTable',
            auto_terminate=['success'])
