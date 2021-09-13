from ..core.Processor import Processor


class PublishMQTT(Processor):
    def __init__(self, schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(PublishMQTT, self).__init__(
            'PublishMQTT',
            properties={
                'Broker URI': 'mqtt-broker:1883',
                'Topic': 'testtopic'},
            auto_terminate=['success', 'failure'],
            schedule=schedule)
