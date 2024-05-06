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

@ENABLE_PYTHON_SCRIPTING
@NEEDS_NUMPY
Feature: MiNiFi can use python modules
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: MiNiFi can use python modules
    Given the example MiNiFi python processors are present
    And a GaussianDistributionWithNumpy processor
    And the scheduling period of the GaussianDistributionWithNumpy processor is set to "10 min"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GaussianDistributionWithNumpy processor is connected to the PutFile

    When the MiNiFi instance starts up
    Then 1 flowfile is placed in the monitored directory in 20 seconds
