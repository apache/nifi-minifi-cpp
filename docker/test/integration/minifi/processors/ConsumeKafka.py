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


from ..core.Processor import Processor


class ConsumeKafka(Processor):
    def __init__(self, context, schedule=None):
        super(ConsumeKafka, self).__init__(
            context=context,
            clazz="ConsumeKafka",
            properties={
                "Kafka Brokers": f"kafka-broker-{context.feature_id}:9092",
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
