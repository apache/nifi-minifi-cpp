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
import sys


def publish_test_mqtt_message(host: str, topic: str, message: str):
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, "docker_test_client_id")
    client.connect(host, 1883, 60)
    client.publish(topic, message)
    client.disconnect()


if __name__ == "__main__":
    if sys.argv.__len__() != 4:
        print("Usage: publish_mqtt_message.py <host> <topic> <message>")
        sys.exit(1)

    publish_test_mqtt_message(sys.argv[1], sys.argv[2], sys.argv[3])
