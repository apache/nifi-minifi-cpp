from ..core.Processor import Processor


class ConsumeKafka(Processor):
    def __init__(self, schedule=None):
        super(ConsumeKafka, self).__init__(
            "ConsumeKafka",
            properties={
                "Kafka Brokers": "kafka-broker:9092",
                "Topic Names": "ConsumeKafkaTest",
                "Topic Name Format": "Names",
                "Honor Transactions": "true",
                "Group ID": "docker_test_group",
                "Offset Reset": "earliest",
                "Key Attribute Encoding": "UTF-8",
                "Message Header Encoding": "UTF-8",
                "Max Poll Time": "4 sec",
                "Session Timeout": "60 sec"},
            auto_terminate=["success"],
            schedule=schedule)
