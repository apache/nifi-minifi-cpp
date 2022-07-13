<!--
  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->
# Kubernetes Examples

The following examples show different configurations that can be applied in Kubernetes for log collection use cases.

## Cluster level log collection with MiNiFi C++

The [daemon-set-log-collection.yml](daemon-set-log-collection/daemon-set-log-collection.yml) file has an example for cluster level log collection, which is done on every node by creating a daemon set.
The config includes a KubernetesControllerService that provides the namespace, pod, uid, container variables for the TailFile processor for getting the logs for the filtered Kubernetes objects.
In this specific example all container logs from the default namespace are collected and forwarded to Kafka.
The controller service can be modified to have additional filters for namespaces, pods, containers, for which more information can be found in the [CONTROLLERS.md](/CONTROLLERS.md#kubernetesControllerService) documentation.

Note: To query Kubernetes cluster information, the MiNiFi agent requires read permission on the pod and namespace objects. One way to give read access to MiNiFi is to create specific cluster roles and cluster role bindings for a specific namespace where the MiNiFi is deployed. There is an example on this in the [daemon-set-log-collection/cluster-roles](daemon-set-log-collection/cluster-roles) directory.

This setup complies with the [node logging agent](https://kubernetes.io/docs/concepts/cluster-administration/logging/#using-a-node-logging-agent) architecture described in the Kubernetes documentation.

## Pod level log collection with sidecar container using MiNiFi C++

The [sidecar-log-collection.yml](sidecar-log-collection/sidecar-log-collection.yml) file has an example for pod level log collection, which is done by creating a sidecar container in the same pod where the container we want to collect the logs from is present. In this specific example a pod with a NiFi container is instantiated with a MiNiFi sidecar container which collects, compresses and uploads the NiFi logs to an AWS S3 bucket.

This setup complies with the [sidecar container with logging agent](https://kubernetes.io/docs/concepts/cluster-administration/logging/#sidecar-container-with-logging-agent) architecture described in the Kubernetes documentation.
