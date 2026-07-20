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

@SUPPORTS_WINDOWS
Feature: Testing custom and default metrics

  Scenario: CustomMetrics(GetFileRs), DefaultMetrics from streaming(DuplicateStreamText) API and DefaultMetrics from buffer(PutFileRs) API
    Given a GetFileRs processor with the "Input Directory" property set to "/tmp/input"
    And a DuplicateStreamText processor
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFileRs processor is connected to the DuplicateStreamText
    And the "success" relationship of the DuplicateStreamText processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated
    And PutFileRs's failure relationship is auto-terminated
    And a directory at "/tmp/input" has a file "hello.txt" with the content "hello"
    And MiNiFi logs processor metrics

    When the MiNiFi instance starts up

    Then at least one file with the content "hheelllloo" is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs match the following regex: "GetFileRsMetrics": {\n[ ]+\"[0-9a-z-]+\": \{\n[ a-zA-Z0-9":,\n]*"BytesRead": "0",[\n ]*"BytesWritten": "5"[ a-zA-Z0-9":,\n]*"InputBytes": "5"[ a-zA-Z0-9":,\n]*}" in less than 10 seconds
    And the Minifi logs match the following regex: "DuplicateStreamTextMetrics": {\n[ ]+\"[0-9a-z-]+\": \{\n[ a-zA-Z0-9":,\n]*"BytesRead": "5",[\n ]*"BytesWritten": "10"[ a-zA-Z0-9":,\n]*}" in less than 10 seconds
    And the Minifi logs match the following regex: "PutFileRsMetrics": {\n[ ]+\"[0-9a-z-]+\": \{\n[ a-zA-Z0-9":,\n]*"BytesRead": "10"[ a-zA-Z0-9":,\n]*}" in less than 10 seconds

    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings
