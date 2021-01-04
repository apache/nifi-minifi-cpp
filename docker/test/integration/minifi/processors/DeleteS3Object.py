from ..core.Processor import Processor

class DeleteS3Object(Processor):
    def __init__(self,
        proxy_host = '',
        proxy_port = '',
        proxy_username = '',
        proxy_password = ''):
            super(DeleteS3Object, self).__init__('DeleteS3Object',
            properties = {
                'Object Key': 'test_object_key',
                'Bucket': 'test_bucket',
                'Access Key': 'test_access_key',
                'Secret Key': 'test_secret',
                'Endpoint Override URL': "http://s3-server:9090",
                'Proxy Host': proxy_host,
                'Proxy Port': proxy_port,
                'Proxy Username': proxy_username,
                'Proxy Password': proxy_password,
            },
            auto_terminate=['success'])

