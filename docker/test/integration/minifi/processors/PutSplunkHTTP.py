from ..core.Processor import Processor


class PutSplunkHTTP(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(PutSplunkHTTP, self).__init__(
            'PutSplunkHTTP',
            properties={
                'Hostname': 'splunk',
                'Port': '8088',
                'Token': 'Splunk 176fae97-f59d-4f08-939a-aa6a543f2485'
            },
            auto_terminate=['success', 'failure'],
            schedule=schedule)
