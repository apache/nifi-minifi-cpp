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

@ENABLE_LLAMACPP
Feature: Run language model inference using LlamaCpp processor

  Scenario: Test inference with a small model
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a RunLlamaCppInference processor with the "Model Path" property set to "/tmp/models/Qwen2-VL-2B-Instruct-Q3_K_M.gguf"
    And the "Prompt" property of the RunLlamaCppInference processor is set to "Repeat after me: banana banana banana"
    And the "Temperature" property of the RunLlamaCppInference processor is set to "0"
    And a LogAttribute processor with the "Log Payload" property set to "true"
    And the "success" relationship of the GenerateFlowFile processor is connected to the RunLlamaCppInference
    And the "success" relationship of the RunLlamaCppInference processor is connected to the LogAttribute

    When all instances start up
    Then the Minifi logs contain the following message: "banana" in less than 60 seconds

  Scenario: Test multimodal inference with a small model
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content from "test-image.png"
    And a RunLlamaCppInference processor with the "Model Path" property set to "/tmp/models/Qwen2-VL-2B-Instruct-Q3_K_M.gguf"
    And the "Prompt" property of the RunLlamaCppInference processor is set to "Output only what is written on the image."
    And the "MultiModal Model Path" property of the RunLlamaCppInference processor is set to "/tmp/models/mmproj-Qwen2-VL-2B-Instruct-f16.gguf"
    And the "Temperature" property of the RunLlamaCppInference processor is set to "0"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the RunLlamaCppInference
    And the "success" relationship of the RunLlamaCppInference processor is connected to the PutFile

    When all instances start up
    Then a single file with the content "minifi" is placed in the "/tmp/output" directory in less than 300 seconds

