# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
    Then a single file with the content "minifi" is placed in the "/tmp/output" directory in less than 600 seconds

