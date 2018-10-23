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

# Apache MiNiFi Docker System Integration Tests

Apache MiNiFi includes a suite of docker-based system integration tests. These
tests are designed to test the integration between distinct MiNiFi instances as
well as other systems which are available in docker, such as Apache NiFi.

* Currently test_https.py does not work due to the upgrade to NiFi 1.7. This will be resolved as
  soon as possible.
  
## Test Execution Lifecycle

Each test involves the following stages as part of its execution lifecycle:

### Definition of flows/Flow DSL

Flows are defined using a python-native domain specific language (DSL). The DSL
supports the standard primitives which make up a NiFi/MiNiFi flow, such as
processors, connections, and controller services. Several processors defined in
the DSL have optional, named parameters enabling concise flow expression.

By default, all relationships are set to auto-terminate. If a relationship is
used, it is automatically taken out of the auto\_terminate list.

**Example Trivial Flow:**

```python
flow = GetFile('/tmp/input') >> LogAttribute() >> PutFile('/tmp/output')
```

#### Supported Processors

The following processors/parameters are supported:

**GetFile**

- input\_dir

**PutFile**

- output\_dir

**LogAttribute**

**ListenHTTP**

- port
- cert=None

**InvokeHTTP**

- url
- method='GET'
- ssl\_context\_service=None

#### Remote Process Groups

Remote process groups and input ports are supported.

**Example InputPort/RemoteProcessGroup:**

```python
port = InputPort('from-minifi', RemoteProcessGroup('http://nifi:8080/nifi'))
```

InputPorts may be used as inputs or outputs in the flow DSL:

```python
recv_flow = (port
             >> LogAttribute()
             >> PutFile('/tmp/output'))

send_flow = (GetFile('/tmp/input')
             >> LogAttribute()
             >> port)
```

These example flows could be deployed as separate NiFi/MiNiFi instances where
the send\_flow would send data to the recv\_flow using the site-to-site
protocol.

### Definition of an output validator

The output validator is responsible for checking the state of a cluster for
valid output conditions. Currently, the only supported output validator is the
SingleFileOutputValidator, which looks for a single file to be written to
/tmp/output by a flow having a given string as its contents.

**Example SingleFileOutputValidator:**

```python
SingleFileOutputValidator('example output')
```

This example SingleFileOutputValidator would validate that a single file is
written with the contents 'example output.'

### Creation of a DockerTestCluster

DockerTestCluster instances are used to deploy one or more flow to a simulated
or actual multi-host docker cluster. This enables testing of interactions
between multiple system components, such as MiNiFi flows. Before the test
cluster is destroyed, an assertion may be performed on the results of the
*check\_output()* method of the cluster. This invokes the validator supplied at
construction against the output state of the system.

Creation of a DockerTestCluster is simple:

**Example DockerTestCluster Instantiation:**

```python
with DockerTestCluster(SingleFileOutputValidator('test')) as cluster:
  ...
  # Perform test operations
  ...
  assert cluster.check_output()
```

Note that a docker cluster must be created inside of a *with* structure to
ensure that all resources are ccreated and destroyed cleanly. 

### Insertion of test input data

Although arbitrary NiFi flows can ingest data from a multitude of sources, a
MiNiFi system integration test is expected to receive input via deterministed,
controlled channels. The primary supported method of providing input to a
MiNiFi system integration test is to insert data into the filesystem at
/tmp/input.

To write a string to the contents of a file in /tmp/input, use the
*put\_test\_data()* method.

**Example put\_test\_data() Usage:**

```python
cluster.put_test_data('test')
```

This writes a file with a random name to /tmp/input, with the contents 'test.'

To provide a resource to a container, such as a TLS certificate, use the
*put\_test\_resource()* method to write a resource file to /tmp/resources.

**Example put\_test\_resource() Usage:**

```python
cluster.put_test_resource('test-resource', 'resource contents')
```

This writes a file to /tmp/resources/test-resource with the contents 'resource
contents.'

### Deployment of one or more flows

Deployment of flows to a test cluster is performed using the *deploy\_flow()*
method of a cluster. Each flow is deployed as a separate docker service having
its own DNS name. If a name is not provided upon deployment, a random name will
be used.

**Example deploy\_flow() Usage:**

```python
cluster.deploy_flow(flow, name='test-flow')
```

The deploy\_flow function defaults to a MiNiFi - C++ engine, but other engines,
such as NiFi may be used:

```python
cluster.deploy_flow(flow, engine='nifi')
```

### Execution of one or more flows

Flows are executed immediately upon deployment and according to schedule
properties defined in the flow.yml. As such, to minimize test latency it is
important to ensure that test inputs are added to the test cluster before flows
are deployed. Filesystem events are monitored using event APIs, ensuring that
flows are executed immediately upon input availability and output is validated
immediately after it is written to disk.

### Output validation

As soon as data is written to /tmp/output, the OutputValidator (defined
according to the documentation above) is executed on the output. The
*check\_output()* cluster method waits for up to 5 seconds for valid output.

### Cluster teardown/cleanup

The deployment of a test cluster involves creating one or more docker
containers and networks, as well as temporary files/directories on the host
system. All resources are cleaned up automatically as long as clusters are
created within a *with* block.

```python

# Using the with block ensures that all cluster resources are cleaned up after
# the test cluster is no longer needed.

with DockerTestCluster(SingleFileOutputValidator('test')) as cluster:
  ...
  # Perform test operations
  ...
  assert cluster.check_output()
```

