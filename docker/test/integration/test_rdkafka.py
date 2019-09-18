# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the \"License\"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an \"AS IS\" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from minifi import *
from minifi.test import *


def test_publish_kafka():
    """
    Verify delivery of message to kafka broker
    """
    producer_flow = GetFile('/tmp/input') >> PublishKafka() >> ('success', LogAttribute())

    with DockerTestCluster(KafkaValidator('test')) as cluster:
        cluster.put_test_data('test')
        cluster.deploy_flow(None, engine='kafka-broker')
        cluster.deploy_flow(producer_flow, name='minifi-producer', engine='minifi-cpp')

        assert cluster.check_output(10)

def test_no_broker():
    """
    Verify failure case when broker is down
    """
    producer_flow = (GetFile('/tmp/input') >> PublishKafka()
                        >> (('failure', PutFile('/tmp/output')),
                            ('success', LogAttribute())))

    with DockerTestCluster(SingleFileOutputValidator('no broker')) as cluster:
        cluster.put_test_data('no broker')
        cluster.deploy_flow(producer_flow, name='minifi-producer', engine='minifi-cpp')

        assert cluster.check_output(30)

def test_broker_on_off():
    """
    Verify delivery of message when broker is unstable
    """
    producer_flow = (GetFile('/tmp/input') >> PublishKafka()
                     >> (('success', PutFile('/tmp/output/success')),
                         ('failure', PutFile('/tmp/output/failure'))))

    with DockerTestCluster(SingleFileOutputValidator('test')) as cluster:
        cluster.put_test_data('test')
        cluster.deploy_flow(None, engine='kafka-broker')
        cluster.deploy_flow(producer_flow, name='minifi-producer', engine='minifi-cpp')

        def start_kafka():
            assert cluster.start_flow('kafka-broker')
            assert cluster.start_flow('kafka-consumer')
        def stop_kafka():
            assert cluster.stop_flow('kafka-consumer')
            assert cluster.stop_flow('kafka-broker')

        assert cluster.check_output(10, dir='/success')
        stop_kafka()
        assert cluster.check_output(30, dir='/failure')
        start_kafka()
        cluster.rm_out_child('/success')
        assert cluster.check_output(30, dir='/success')
        stop_kafka()
        cluster.rm_out_child('/failure')
        assert cluster.check_output(30, dir='/failure')

