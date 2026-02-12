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

@CORE
Feature: DefragmentText can defragment fragmented data from TailFile

  Scenario Outline: DefragmentText correctly merges split messages from TailFile multiple file tail-mode
    Given a TailFile processor with the name "MultiTail" and the "File to Tail" property set to "test_file_.*\.log"
    And the "tail-base-directory" property of the MultiTail processor is set to "/tmp/input"
    And the "tail-mode" property of the MultiTail processor is set to "Multiple file"
    And the "Initial Start Position" property of the MultiTail processor is set to "Beginning of File"
    And the "Input Delimiter" property of the MultiTail processor is set to "%"
    And a file with filename "test_file_one.log" and content "<input_one>" is present in "/tmp/input"
    And a file with filename "test_file_two.log" and content "<input_two>" is present in "/tmp/input"
    And a DefragmentText processor with the "Pattern" property set to "<pattern>"
    And the "Pattern Location" property of the DefragmentText processor is set to "<pattern location>"
    And DefragmentText is EVENT_DRIVEN
    And a PutFile processor with the name "SuccessPut" and the "Directory" property set to "/tmp/output"
    And SuccessPut is EVENT_DRIVEN
    And the "success" relationship of the MultiTail processor is connected to the DefragmentText
    And the "success" relationship of the DefragmentText processor is connected to the SuccessPut
    And SuccessPut's success relationship is auto-terminated

    When all instances start up
    Then files with contents "<success_flow_files>" are placed in the "/tmp/output" directory in less than 60 seconds

    Examples:
      | input_one                                    | input_two                                        | pattern       | pattern location | success_flow_files                                                          |
      | <1>cat%dog%mouse%<2>apple%banana%<3>English% | <1>Katze%Hund%Maus%<2>Apfel%Banane%<3>Deutsch%   | <[0-9]+>      | Start of Message | <1>cat%dog%mouse%,<1>Katze%Hund%Maus%,<2>apple%banana%,<2>Apfel%Banane%       |
      | <1>cat%dog%mouse%<2>apple%banana%<3>English% | <1>Katze%Hund%Maus%<2>Apfel%Banane%<3>Deutsch%   | <[0-9]+>      | End of Message   | <1>,<1>,cat%dog%mouse%<2>,Katze%Hund%Maus%<2>,apple%banana%<3>,Apfel%Banane%<3>   |

  Scenario Outline: DefragmentText correctly merges split messages from multiple TailFile
    Given a TailFile processor with the name "TailOne" and the "File to Tail" property set to "/tmp/input/test_file_one.log"
    And the "Initial Start Position" property of the TailOne processor is set to "Beginning of File"
    And the "Input Delimiter" property of the TailOne processor is set to "%"
    And a TailFile processor with the name "TailTwo" and the "File to Tail" property set to "/tmp/input/test_file_two.log"
    And the "Initial Start Position" property of the TailTwo processor is set to "Beginning of File"
    And the "Input Delimiter" property of the TailTwo processor is set to "%"
    And a file with filename "test_file_one.log" and content "<input_one>" is present in "/tmp/input"
    And a file with filename "test_file_two.log" and content "<input_two>" is present in "/tmp/input"
    And a DefragmentText processor with the "Pattern" property set to "<pattern>"
    And the "Pattern Location" property of the DefragmentText processor is set to "<pattern location>"
    And DefragmentText is EVENT_DRIVEN
    And a PutFile processor with the name "SuccessPut" and the "Directory" property set to "/tmp/output"
    And SuccessPut is EVENT_DRIVEN
    And the "success" relationship of the TailOne processor is connected to the DefragmentText
    And the "success" relationship of the TailTwo processor is connected to the DefragmentText
    And the "success" relationship of the DefragmentText processor is connected to the SuccessPut
    And SuccessPut's success relationship is auto-terminated

    When all instances start up
    Then files with contents "<success_flow_files>" are placed in the "/tmp/output" directory in less than 60 seconds

    Examples:
      | input_one                                    | input_two                                        | pattern       | pattern location | success_flow_files                                                          |
      | <1>cat%dog%mouse%<2>apple%banana%<3>English% | <1>Katze%Hund%Maus%<2>Apfel%Banane%<3>Deutsch%   | <[0-9]+>      | Start of Message | <1>cat%dog%mouse%,<1>Katze%Hund%Maus%,<2>apple%banana%,<2>Apfel%Banane%       |
      | <1>cat%dog%mouse%<2>apple%banana%<3>English% | <1>Katze%Hund%Maus%<2>Apfel%Banane%<3>Deutsch%   | <[0-9]+>      | End of Message   | <1>,<1>,cat%dog%mouse%<2>,Katze%Hund%Maus%<2>,apple%banana%<3>,Apfel%Banane%<3>   |

  Scenario Outline: DefragmentText merges split messages from a single TailFile
    Given a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And the "Initial Start Position" property of the TailFile processor is set to "Beginning of File"
    And the "Input Delimiter" property of the TailFile processor is set to "%"
    And a file with filename "test_file.log" and content "<input>" is present in "/tmp/input"
    And a DefragmentText processor with the "Pattern" property set to "<pattern>"
    And the "Pattern Location" property of the DefragmentText processor is set to "<pattern location>"
    And DefragmentText is EVENT_DRIVEN
    And a PutFile processor with the name "SuccessPut" and the "Directory" property set to "/tmp/output"
    And SuccessPut is EVENT_DRIVEN
    And the "success" relationship of the TailFile processor is connected to the DefragmentText
    And the "success" relationship of the DefragmentText processor is connected to the SuccessPut
    And SuccessPut's success relationship is auto-terminated

    When all instances start up
    Then files with contents "<success_flow_files>" are placed in the "/tmp/output" directory in less than 30 seconds

    Examples:
      | input                                                        | pattern       | pattern location |  success_flow_files                                   |
      | <1> apple%banana%<2> foo%bar%baz%<3> cat%dog%                | <[0-9]+>      | Start of Message | <1> apple%banana%,<2> foo%bar%baz%                    |
      | <1> apple%banana%<2> foo%bar%baz%<3> cat%dog%                | <[0-9]+>      | End of Message   | <1>, apple%banana%<2>, foo%bar%baz%<3>                |
      | <1> apple%banana%<2> foo%bar%baz%<3> cat%dog%                | %             | Start of Message | <1> apple,%banana,%<2> foo,%bar,%baz,%<3> cat,%dog    |
      | <1> apple%banana%<2> foo%bar%baz%<3> cat%dog%                | %             | End of Message   | <1> apple%,banana%,<2> foo%,bar%,baz%,<3> cat%,dog%   |
      | previous%>>a<< apple%banana%>>b<< foo%bar%baz%>>c<< cat%dog% | >>[a-z]+<<    | Start of Message | previous%,>>a<< apple%banana%,>>b<< foo%bar%baz%      |
      | previous%>>a<< apple%banana%>>b<< foo%bar%baz%>>c<< cat%dog% | >>[a-z]+<<    | End of Message   | previous%>>a<<, apple%banana%>>b<<, foo%bar%baz%>>c<< |
      | long%message%with%a%single%delimiter%at%the%end%!%           | !             | Start of Message | long%message%with%a%single%delimiter%at%the%end%      |
      | long%message%with%a%single%delimiter%at%the%end%!%           | !             | End of Message   | long%message%with%a%single%delimiter%at%the%end%!     |
