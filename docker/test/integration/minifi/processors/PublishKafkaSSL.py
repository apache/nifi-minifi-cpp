from ..core.Processor import Processor

class PublishKafkaSSL(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(PublishKafkaSSL, self).__init__('PublishKafka',
            properties={'Client Name': 'LMN', 'Known Brokers': 'kafka-broker:9093',
                'Topic Name': 'test', 'Batch Size': '10',
                'Compress Codec': 'none', 'Delivery Guarantee': '1',
                'Request Timeout': '10 sec', 'Message Timeout': '12 sec',
                'Security CA': '/tmp/resources/certs/ca-cert',
                'Security Cert': '/tmp/resources/certs/client_LMN_client.pem',
                'Security Pass Phrase': 'abcdefgh',
                'Security Private Key': '/tmp/resources/certs/client_LMN_client.key',
                'Security Protocol': 'ssl'},
            auto_terminate=['success'],
            schedule=schedule)
