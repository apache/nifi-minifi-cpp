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

@x86_x64_only
@CORE
Feature: Minifi C++ can act as a modbus tcp master

  Scenario: MiNiFi can fetch data from a modbus slave
    Given a FetchModbusTcp processor
    And a JsonRecordSetWriter controller service is set up and the "Output Grouping" property set to "One Line Per Object"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And PutFile is EVENT_DRIVEN
    And the "Unit Identifier" property of the FetchModbusTcp processor is set to "255"
    And the "Record Set Writer" property of the FetchModbusTcp processor is set to "JsonRecordSetWriter"
    And the "Hostname" property of the FetchModbusTcp processor is set to "diag-slave-tcp-${scenario_id}"
    And there is an accessible PLC with modbus enabled
    And PLC register has been set with h@52=123 command
    And PLC register has been set with h@5678/f=1.75 command
    And PLC register has been set with h@4444=77 command
    And PLC register has been set with h@4445=105 command
    And PLC register has been set with h@4446=78 command
    And PLC register has been set with h@4447=105 command
    And PLC register has been set with h@4448=70 command
    And PLC register has been set with h@4449=105 command

    And the "success" relationship of the FetchModbusTcp processor is connected to the PutFile
    And the "foo" property of the FetchModbusTcp processor is set to "holding-register:52"
    And the "bar" property of the FetchModbusTcp processor is set to "405678:REAL"
    And the "baz" property of the FetchModbusTcp processor is set to "4x4444:CHAR[6]"
    And PutFile's success relationship is auto-terminated

    When the MiNiFi instance starts up
    Then at least one file with the JSON content "{"foo":123,"bar":1.75,"baz":["M", "i", "N", "i", "F", "i"]}" is placed in the "/tmp/output" directory in less than 10 seconds
