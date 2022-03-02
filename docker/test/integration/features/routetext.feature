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

Feature: Processing log files line-by-line using RouteText
  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Write different level of logs to different files
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with filename "test_file.log" and content "[INFO] one\n[WARNING] two\n[INFO] three\n[WARNING] four\n" is present in "/tmp/input"
    And a RouteText processor with the "Routing Strategy" property set to "Dynamic Routing"
    And the "Matching Strategy" property of the RouteText processor is set to "Starts With"
    And the "Info" property of the RouteText processor is set to "[INFO]"
    And the "Warning" property of the RouteText processor is set to "[WARNING]"
    And a UpdateAttribute processor with the name "UpdateInfo" and the "filename" property set to "info.txt"
    And a UpdateAttribute processor with the name "UpdateWarning" and the "filename" property set to "warning.txt"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the RouteText
    And the "Info" relationship of the RouteText processor is connected to the UpdateInfo
    And the "Warning" relationship of the RouteText processor is connected to the UpdateWarning
    And the "success" relationship of the UpdateInfo processor is connected to the PutFile
    And the "success" relationship of the UpdateWarning processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then two flowfiles with the contents "[INFO] one\n[INFO] three\n" and "[WARNING] two\n[WARNING] four\n" are placed in the monitored directory in less than 10 seconds
