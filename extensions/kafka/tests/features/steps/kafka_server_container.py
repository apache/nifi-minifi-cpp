import logging

from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class KafkaServer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("apache/kafka:latest", f"kafka-server-{test_context.scenario_id}", test_context.network)

    def deploy(self):
        return super().deploy()

    def create_topic(self, topic_name: str):
        (code, output) = self.exec_run(["/bin/bash", "-c", f"/opt/kafka/bin/kafka-topics.sh --create --topic {topic_name} --bootstrap-server localhost:9092"])
        logging.info(output)
        return code == 0

    def produce_message(self, topic_name: str, message: str):
        (code, output) = self.exec_run(["/bin/bash", "-c", f"/opt/kafka/bin/kafka-console-producer.sh --topic {topic_name} --bootstrap-server localhost:9092 <<< '{message}'"])
        logging.info(output)
        return code == 0

    def produce_message_with_key(self, topic_name: str, message: str, message_key: str):
        (code, output) = self.exec_run(["/bin/bash", "-c", f"/opt/kafka/bin/kafka-console-producer.sh --property 'key.separator=:' --property 'parse.key=true' --topic {topic_name} --bootstrap-server localhost:9092 <<< '{message_key}:{message}'"])
        logging.info(output)
        return code == 0

    def run_python_in_kafka_helper_docker(self, command: str):
        output = self.client.containers.run("minifi-kafka-helper:latest", ["python", "-c", command], remove=True, stdout=True, stderr=True, network=self.network.name)
        logging.info(output)
        return True
