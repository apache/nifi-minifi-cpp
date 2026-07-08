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
Feature: ImageToTensor preprocesses image bytes into normalised tensors

  Scenario: RGB CHW 224x224 tensor is produced with the expected shape and dtype
    Given a host resource file "grace_hopper.jpg" is copied to the "/tmp/input/grace_hopper.jpg" path in the MiNiFi container
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "false"
    And an ImageToTensor processor with the "Target width" property set to "224"
    And the "Target height" property of the ImageToTensor processor is set to "224"
    And the "Resize filter" property of the ImageToTensor processor is set to "Bilinear"
    And the "Resize mode" property of the ImageToTensor processor is set to "Stretch"
    And the "Color format" property of the ImageToTensor processor is set to "RGB"
    And the "Tensor shape format" property of the ImageToTensor processor is set to "CHW"
    And the "Mean" property of the ImageToTensor processor is set to "0.485, 0.456, 0.406"
    And the "Standard Deviation" property of the ImageToTensor processor is set to "0.229, 0.224, 0.225"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the ImageToTensor
    And the "success" relationship of the ImageToTensor processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    And ImageToTensor's failure relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "key:tensor.0.shape value:1,3,224,224" in less than 30 seconds
    And the Minifi logs contain the following message: "key:tensor.0.dtype value:F32" in less than 1 seconds
    And the Minifi logs do not contain errors

  Scenario: HWC layout is reflected in the tensor.shape attribute
    Given a host resource file "grace_hopper.jpg" is copied to the "/tmp/input/grace_hopper.jpg" path in the MiNiFi container
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "false"
    And an ImageToTensor processor with the "Target width" property set to "300"
    And the "Target height" property of the ImageToTensor processor is set to "300"
    And the "Tensor shape format" property of the ImageToTensor processor is set to "HWC"
    And the "Color format" property of the ImageToTensor processor is set to "RGB"
    And the "Mean" property of the ImageToTensor processor is set to "0.0"
    And the "Standard Deviation" property of the ImageToTensor processor is set to "255.0"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the ImageToTensor
    And the "success" relationship of the ImageToTensor processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    And ImageToTensor's failure relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "key:tensor.0.shape value:1,300,300,3" in less than 30 seconds
    And the Minifi logs do not contain errors

  Scenario: Letterbox mode still produces the target dimensions
    Given a host resource file "grace_hopper.jpg" is copied to the "/tmp/input/grace_hopper.jpg" path in the MiNiFi container
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "false"
    And an ImageToTensor processor with the "Target width" property set to "320"
    And the "Target height" property of the ImageToTensor processor is set to "240"
    And the "Resize mode" property of the ImageToTensor processor is set to "Letterbox"
    And the "Letterbox pad value" property of the ImageToTensor processor is set to "0.0"
    And the "Color format" property of the ImageToTensor processor is set to "RGB"
    And the "Tensor shape format" property of the ImageToTensor processor is set to "CHW"
    And the "Mean" property of the ImageToTensor processor is set to "127.0"
    And the "Standard Deviation" property of the ImageToTensor processor is set to "128.0"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN
    And the "success" relationship of the GetFile processor is connected to the ImageToTensor
    And the "success" relationship of the ImageToTensor processor is connected to the LogAttribute
    And LogAttribute's success relationship is auto-terminated
    And ImageToTensor's failure relationship is auto-terminated

    When the MiNiFi instance starts up

    Then the Minifi logs contain the following message: "key:tensor.0.shape value:1,3,240,320" in less than 30 seconds
    And the Minifi logs do not contain errors

  Scenario: Invalid image bytes route to failure without crashing MiNiFi
    Given a directory at "/tmp/input" has a file "not_an_image.bin" with the content "garbage bytes not a real image"
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "false"
    And an ImageToTensor processor with the "Target width" property set to "224"
    And the "Target height" property of the ImageToTensor processor is set to "224"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the ImageToTensor
    And the "failure" relationship of the ImageToTensor processor is connected to the PutFile
    And ImageToTensor's success relationship is auto-terminated
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When the MiNiFi instance starts up

    Then at least one file with the content "garbage bytes not a real image" is placed in the "/tmp/output" directory in less than 30 seconds
