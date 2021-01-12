from ..core.Processor import Processor

class ListenHTTP(Processor):
    def __init__(self, cert=None, schedule=None):
        properties = {}

        if cert is not None:
            properties['SSL Certificate'] = cert
            properties['SSL Verify Peer'] = 'no'

        super(ListenHTTP, self).__init__('ListenHTTP',
			properties=properties,
			auto_terminate=['success'],
			schedule=schedule)
