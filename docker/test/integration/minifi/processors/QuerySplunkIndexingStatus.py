from ..core.Processor import Processor


class QuerySplunkIndexingStatus(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN',
                                 'penalization period': '1 sec'}):
        super(QuerySplunkIndexingStatus, self).__init__(
            'QuerySplunkIndexingStatus',
            properties={
                'Hostname': 'splunk',
                'Port': '8088',
                'Token': 'Splunk 176fae97-f59d-4f08-939a-aa6a543f2485'
            },
            auto_terminate=['acknowledged', 'unacknowledged', 'undetermined', 'failure'],
            schedule=schedule)
