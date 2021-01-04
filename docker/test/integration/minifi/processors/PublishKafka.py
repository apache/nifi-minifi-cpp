from ..core.Processor import Processor

class PublishKafka(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(PublishKafka, self).__init__('PublishKafka',
			properties={'Client Name': 'nghiaxlee', 'Known Brokers': 'kafka-broker:9092', 'Topic Name': 'test',
				'Batch Size': '10', 'Compress Codec': 'none', 'Delivery Guarantee': '1',
				'Request Timeout': '10 sec', 'Message Timeout': '12 sec'},
			auto_terminate=['success'],
			schedule=schedule)
