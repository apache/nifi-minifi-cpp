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
Feature: Changing flowfile contents using the ReplaceText processor

  Scenario Outline: Replace text using Entire text mode
    Given a GenerateFlowFile processor with the "Custom Text" property set to "<input>"
    And the scheduling period of the GenerateFlowFile processor is set to "1 hour"
    And the "Data Format" property of the GenerateFlowFile processor is set to "Text"
    And the "Unique FlowFiles" property of the GenerateFlowFile processor is set to "false"
    And a ReplaceText processor with the "Evaluation Mode" property set to "Entire text"
    And the "Replacement Strategy" property of the ReplaceText processor is set to "<replacement_strategy>"
    And the "Search Value" property of the ReplaceText processor is set to "<search_value>"
    And the "Replacement Value" property of the ReplaceText processor is set to "<replacement_value>"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GenerateFlowFile processor is connected to the ReplaceText
    And the "success" relationship of the ReplaceText processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then there is a single file with "<output>" content in the "/tmp/output" directory in less than 10 seconds

    Examples:
      | input                 | replacement_strategy | search_value | replacement_value | output                    |
      | apple                 | Prepend              | _            | pine              | pineapple                 |
      | apple                 | Append               | _            | sauce             | applesauce                |
      | one apple, two apples | Regex Replace        | a([a-z]+)e   | ri$1et            | one ripplet, two ripplets |
      | one apple, two apples | Literal Replace      | apple        | banana            | one banana, two bananas   |
      | one apple, two apples | Always Replace       | _            | banana            | banana                    |

  Scenario Outline: Replace text using Line-by-Line mode
    Given a directory at "/tmp/input" has a file with the content "<input>"
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a ReplaceText processor with the "Evaluation Mode" property set to "Line-by-Line"
    And the "Line-by-Line Evaluation Mode" property of the ReplaceText processor is set to "<evaluation_mode>"
    And the "Replacement Strategy" property of the ReplaceText processor is set to "Regex Replace"
    And the "Search Value" property of the ReplaceText processor is set to "a+(b+)c+"
    And the "Replacement Value" property of the ReplaceText processor is set to "_$1_"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the ReplaceText
    And the "success" relationship of the ReplaceText processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then there is a single file with "<output>" content in the "/tmp/output" directory in less than 10 seconds

    Examples:
      | input                      | evaluation_mode      | output                     |
      | abc\n aabbcc\n aaabbbccc\n | All                  | _b_\n _bb_\n _bbb_\n       |
      | abc\n aabbcc\n aaabbbccc   | All                  | _b_\n _bb_\n _bbb_         |
      | abc\n aabbcc\n aaabbbccc\n | First-Line           | _b_\n aabbcc\n aaabbbccc\n |
      | abc\n aabbcc\n aaabbbccc\n | Last-Line            | abc\n aabbcc\n _bbb_\n     |
      | abc\n aabbcc\n aaabbbccc\n | Except-First-Line    | abc\n _bb_\n _bbb_\n       |
      | abc\n aabbcc\n aaabbbccc\n | Except-Last-Line     | _b_\n _bb_\n aaabbbccc\n   |
