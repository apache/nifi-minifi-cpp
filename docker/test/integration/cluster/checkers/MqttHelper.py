# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import paho.mqtt.client as mqtt
from .sparkplug_b_pb2 import Payload


class MqttHelper:
    def publish_test_sparkplug_payload(self, topic: str):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, "docker_test_client_id")
        client.connect("localhost", 1883, 60)

        payload = Payload()
        metric = payload.metrics.add()
        metric.name = "TestMetric"
        metric.int_value = 123
        metric.timestamp = 45345346346
        payload.uuid = "test-uuid"
        payload.timestamp = 987654321
        payload.seq = 12345
        payload.body = b"test-body"
        payload_bytes = payload.SerializeToString()

        client.publish(topic, payload_bytes)
        client.disconnect()
