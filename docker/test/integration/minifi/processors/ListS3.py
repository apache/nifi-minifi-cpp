from ..core.Processor import Processor


class ListS3(Processor):
    def __init__(self,
                 proxy_host='',
                 proxy_port='',
                 proxy_username='',
                 proxy_password=''):
        super(ListS3, self).__init__('ListS3',
                                     properties={
                                         'Bucket': 'test_bucket',
                                         'Access Key': 'test_access_key',
                                         'Secret Key': 'test_secret',
                                         'Endpoint Override URL': "http://s3-server:9090",
                                         'Proxy Host': proxy_host,
                                         'Proxy Port': proxy_port,
                                         'Proxy Username': proxy_username,
                                         'Proxy Password': proxy_password,
                                     },
                                     schedule={'scheduling period': '3 sec'},
                                     auto_terminate=['success'])
