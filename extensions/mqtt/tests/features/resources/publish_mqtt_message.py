# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
import sys

import paho.mqtt.client as mqtt


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
