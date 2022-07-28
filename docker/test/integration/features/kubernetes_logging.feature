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

@requires.kubernetes.cluster
Feature: TailFile can collect logs from Kubernetes pods

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Collect all logs from the default namespace
    Given a TailFile processor in a Kubernetes cluster
    And the "tail-mode" property of the TailFile processor is set to "Multiple file"
    And the "tail-base-directory" property of the TailFile processor is set to "/var/log/pods/${namespace}_${pod}_${uid}/${container}"
    And the "File to Tail" property of the TailFile processor is set to ".*\.log"
    And the "Lookup frequency" property of the TailFile processor is set to "1s"
    And the TailFile processor has an Attribute Provider Service which is a Kubernetes Controller Service
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the TailFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then two flowfiles with the contents "Hello World!" and "Hello again, World!" are placed in the monitored directory in less than 30 seconds

  Scenario: Collect logs from selected pods
    Given a TailFile processor in a Kubernetes cluster
    And the "tail-mode" property of the TailFile processor is set to "Multiple file"
    And the "tail-base-directory" property of the TailFile processor is set to "/var/log/pods/${namespace}_${pod}_${uid}/${container}"
    And the "File to Tail" property of the TailFile processor is set to ".*\.log"
    And the "Lookup frequency" property of the TailFile processor is set to "1s"
    And the TailFile processor has an Attribute Provider Service which is a Kubernetes Controller Service with the "Pod Name Filter" property set to ".*one"
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the TailFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then one flowfile with the contents "Hello World!" is placed in the monitored directory in less than 30 seconds

  Scenario: Collect logs from selected containers
    Given a TailFile processor in a Kubernetes cluster
    And the "tail-mode" property of the TailFile processor is set to "Multiple file"
    And the "tail-base-directory" property of the TailFile processor is set to "/var/log/pods/${namespace}_${pod}_${uid}/${container}"
    And the "File to Tail" property of the TailFile processor is set to ".*\.log"
    And the "Lookup frequency" property of the TailFile processor is set to "1s"
    And the TailFile processor has an Attribute Provider Service which is a Kubernetes Controller Service with the "Container Name Filter" property set to "echo-[^o].."
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the TailFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then one flowfile with the contents "Hello again, World!" is placed in the monitored directory in less than 30 seconds

  Scenario: Pod name etc are added as flow file attributes
    Given a TailFile processor in a Kubernetes cluster
    And the "tail-mode" property of the TailFile processor is set to "Multiple file"
    And the "tail-base-directory" property of the TailFile processor is set to "/var/log/pods/${namespace}_${pod}_${uid}/${container}"
    And the "File to Tail" property of the TailFile processor is set to ".*\.log"
    And the "Lookup frequency" property of the TailFile processor is set to "1s"
    And the TailFile processor has an Attribute Provider Service which is a Kubernetes Controller Service with the "Pod Name Filter" property set to ".*one"
    And a LogAttribute processor in the Kubernetes cluster
    And the "success" relationship of the TailFile processor is connected to the LogAttribute
    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "key:kubernetes.namespace value:default" in less than 30 seconds
    And the Minifi logs contain the following message: "key:kubernetes.pod value:hello-world-one" in less than 1 second
    And the Minifi logs contain the following message: "key:kubernetes.uid value:" in less than 1 second
    And the Minifi logs contain the following message: "key:kubernetes.container value:echo-one" in less than 1 second
