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
@ENABLE_KUBERNETES
Feature: Minifi can collect metrics from Kubernetes pods

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Collect all metrics from the default namespace
    Given a CollectKubernetesPodMetrics processor in a Kubernetes cluster
    And the CollectKubernetesPodMetrics processor has a Kubernetes Controller Service which is a Kubernetes Controller Service
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the CollectKubernetesPodMetrics processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then at least one flowfile with the content '"kind":"PodMetricsList","apiVersion":"metrics.k8s.io/v1beta1"' is placed in the monitored directory in less than 2 minutes

  Scenario: Collect metrics from selected pods
    Given a CollectKubernetesPodMetrics processor in a Kubernetes cluster
    And the CollectKubernetesPodMetrics processor has a Kubernetes Controller Service which is a Kubernetes Controller Service with the "Pod Name Filter" property set to ".*one"
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the CollectKubernetesPodMetrics processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then at least one flowfile with the content '"metadata":{"name":"hello-world-one","namespace":"default"' is placed in the monitored directory in less than 2 minutes

  Scenario: Collect metrics from selected containers
    Given a CollectKubernetesPodMetrics processor in a Kubernetes cluster
    And the CollectKubernetesPodMetrics processor has a Kubernetes Controller Service which is a Kubernetes Controller Service with the "Container Name Filter" property set to "echo-[^o].."
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the CollectKubernetesPodMetrics processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then at least one flowfile with the content '"containers":[{"name":"echo-two","usage":{"cpu":"0","memory":' is placed in the monitored directory in less than 2 minutes
