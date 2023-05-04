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

* Currently there is an extra unused test mockup for testing TLS with invoke_http.
* HashContent tests do not actually seem what they advertise to
* There is a test requirement for PublishKafka, confirming it can handle broker outages. This will be reintroduced when ConsumeKafka is on the master and will have its similar testing requirements implemented.

## Test environment

The test framework is written in Python 3 and uses pip3 to add required packages. The framework it uses is python-behave, a BDD testing framework. The feature specifications are written in human readable format in the features directory. Please refer to the behave documentation on how the framework performs testing.

The tests use docker containers so docker engine should be installed on your system. Check the [get docker](https://docs.docker.com/get-docker/) page for further information.

One of the required python packages is the `m2crypto` package which depends on `swig` for compilation,
so `swig` should also be installed on your system (e.g. `sudo apt install swig` on debian based systems).

### Execution of one or more flows

Flows are executed immediately upon deployment and according to schedule
properties defined in the flow.yml. As such, to minimize test latency it is
important to ensure that test inputs are added to the test cluster before flows
are deployed. Filesystem events are monitored using event APIs, ensuring that
flows are executed immediately upon input availability and output is validated
immediately after it is written to disk.

