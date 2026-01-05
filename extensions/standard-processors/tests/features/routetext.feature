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
Feature: Processing log files line-by-line using RouteText

  Scenario: Write different level of logs to different files
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "[INFO] one\n[WARNING] two\n[INFO] three\n[WARNING] four\n"
    And a RouteText processor with the "Routing Strategy" property set to "Dynamic Routing"
    And the "Matching Strategy" property of the RouteText processor is set to "Starts With"
    And the "Info" property of the RouteText processor is set to "[INFO]"
    And the "Warning" property of the RouteText processor is set to "[WARNING]"
    And a UpdateAttribute processor with the name "UpdateInfo" and the "filename" property set to "info.txt"
    And a UpdateAttribute processor with the name "UpdateWarning" and the "filename" property set to "warning.txt"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And RouteText's original relationship is auto-terminated
    And PutFile's success relationship is auto-terminated
    And the "success" relationship of the GetFile processor is connected to the RouteText
    And the "Info" relationship of the RouteText processor is connected to the UpdateInfo
    And the "Warning" relationship of the RouteText processor is connected to the UpdateWarning
    And the "success" relationship of the UpdateInfo processor is connected to the PutFile
    And the "success" relationship of the UpdateWarning processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then files with contents "[INFO] one\n[INFO] three\n" and "[WARNING] two\n[WARNING] four\n" are placed in the "/tmp/output" directory in less than 15 seconds
