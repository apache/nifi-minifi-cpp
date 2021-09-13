from ..core.Processor import Processor


class ConsumeMQTT(Processor):
    def __init__(self, schedule={'scheduling strategy': 'TIMER_DRIVEN'}):
        super(ConsumeMQTT, self).__init__(
            'ConsumeMQTT',
            properties={
                'Broker URI': 'mqtt-broker:1883',
                'Topic': 'testtopic'},
            auto_terminate=['success'],
            schedule=schedule)
