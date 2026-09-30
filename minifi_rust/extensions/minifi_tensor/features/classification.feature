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
Feature: Image classification with MobileNetV2

  # Based on https://github.com/sonos/tract/tree/main/examples/onnx-mobilenet-v2
  Scenario: Grace Hopper image is classified using MobileNetV2 ONNX and imagenet labels (ImageToTensor + InvokeTract + ClassifyOutput)
    Given a host resource file "grace_hopper.jpg" is copied to the "/tmp/input/grace_hopper.jpg" path in the MiNiFi container
    And a host resource file "mobilenetv2-7.onnx" is copied to the "/tmp/models/mobilenetv2-7.onnx" path in the MiNiFi container
    And a host resource file "imagenet_slim_labels.txt" is copied to the "/tmp/models/imagenet_slim_labels.txt" path in the MiNiFi container

    And a TractModelService controller service named "MobileNet" is set up and the "Model File Path" property set to "/tmp/models/mobilenetv2-7.onnx"
    And the "Model format" property of the MobileNet controller service is set to "Onnx"

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
    And the "Pixel divisor" property of the ImageToTensor processor is set to "255"

    And an InvokeTractModel processor with the "Tract model service" property set to "MobileNet"

    And a ClassifyOutput processor with the "Top K" property set to "3"
    And the "Score activation" property of the ClassifyOutput processor is set to "Softmax"
    And the "Labels file path" property of the ClassifyOutput processor is set to "/tmp/models/imagenet_slim_labels.txt"
    # ImageNet slim labels start with a dummy entry on line 0
    And the "Label index offset" property of the ClassifyOutput processor is set to "1"
    And the "Confidence Threshold" property of the ClassifyOutput processor is set to "0.0"

    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN

    And the "success" relationship of the GetFile processor is connected to the ImageToTensor
    And the "success" relationship of the ImageToTensor processor is connected to the InvokeTractModel
    And the "success" relationship of the InvokeTractModel processor is connected to the ClassifyOutput
    And the "success" relationship of the ClassifyOutput processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the PutFile
    And ImageToTensor's failure relationship is auto-terminated
    And InvokeTractModel's failure relationship is auto-terminated
    And ClassifyOutput's failure relationship is auto-terminated
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When the MiNiFi instance starts up

    # Grace Hopper wears a US Navy uniform; MobileNetV2 on ImageNet consistently
    # picks "military uniform" (occasionally "suit", "Windsor tie", or
    # "bulletproof vest" as close runner-ups). Match any of those to keep the
    # test resilient to small numeric differences across tract versions.
    Then the Minifi logs match the following regex: "key:class.top1.name value:(military uniform|bulletproof vest|suit|Windsor tie)" in less than 60 seconds
    And the Minifi logs match the following regex: "key:class.count value:[1-3]" in less than 1 seconds
    And the Minifi logs contain the following message: "key:mime.type value:application/json" in less than 1 seconds
    And at least one file in "/tmp/output" content match the following regex: "\"class_name\":\"(military uniform|bulletproof vest|suit|Windsor tie)\"" in less than 30 seconds
    And the Minifi logs do not contain errors

  # Based on https://github.com/sonos/tract/tree/main/examples/onnx-mobilenet-v2
  Scenario: Grace Hopper image is classified using MobileNetV2 ONNX and imagenet labels (ClassifyImage)
    Given a host resource file "grace_hopper.jpg" is copied to the "/tmp/input/grace_hopper.jpg" path in the MiNiFi container
    And a host resource file "mobilenetv2-7.onnx" is copied to the "/tmp/models/mobilenetv2-7.onnx" path in the MiNiFi container
    And a host resource file "imagenet_slim_labels.txt" is copied to the "/tmp/models/imagenet_slim_labels.txt" path in the MiNiFi container

    And a TractModelService controller service named "MobileNet" is set up and the "Model File Path" property set to "/tmp/models/mobilenetv2-7.onnx"
    And the "Model format" property of the MobileNet controller service is set to "Onnx"

    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "false"

    And a ClassifyImage processor with the "Target width" property set to "224"
    And the "Target height" property of the ClassifyImage processor is set to "224"
    And the "Resize filter" property of the ClassifyImage processor is set to "Bilinear"
    And the "Resize mode" property of the ClassifyImage processor is set to "Stretch"
    And the "Color format" property of the ClassifyImage processor is set to "RGB"
    And the "Tensor shape format" property of the ClassifyImage processor is set to "CHW"
    And the "Mean" property of the ClassifyImage processor is set to "0.485, 0.456, 0.406"
    And the "Standard Deviation" property of the ClassifyImage processor is set to "0.229, 0.224, 0.225"
    And the "Pixel divisor" property of the ClassifyImage processor is set to "255"
    And the "Tract model service" property of the ClassifyImage processor is set to "MobileNet"
    And the "Top K" property of the ClassifyImage processor is set to "3"
    And the "Score activation" property of the ClassifyImage processor is set to "Softmax"
    And the "Labels file path" property of the ClassifyImage processor is set to "/tmp/models/imagenet_slim_labels.txt"
    And the "Label index offset" property of the ClassifyImage processor is set to "1"
    And the "Confidence Threshold" property of the ClassifyImage processor is set to "0.0"

    And a PutFile processor with the "Directory" property set to "/tmp/output"

    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And LogAttribute is EVENT_DRIVEN

    And the "success" relationship of the GetFile processor is connected to the ClassifyImage
    And the "success" relationship of the ClassifyImage processor is connected to the LogAttribute
    And the "success" relationship of the LogAttribute processor is connected to the PutFile
    And ClassifyImage's failure relationship is auto-terminated
    And PutFile's success relationship is auto-terminated
    And PutFile's failure relationship is auto-terminated

    When the MiNiFi instance starts up

    # Grace Hopper wears a US Navy uniform; MobileNetV2 on ImageNet consistently
    # picks "military uniform" (occasionally "suit", "Windsor tie", or
    # "bulletproof vest" as close runner-ups). Match any of those to keep the
    # test resilient to small numeric differences across tract versions.
    Then the Minifi logs match the following regex: "key:class.top1.name value:(military uniform|bulletproof vest|suit|Windsor tie)" in less than 60 seconds
    And the Minifi logs match the following regex: "key:class.count value:[1-3]" in less than 1 seconds
    And the Minifi logs contain the following message: "key:mime.type value:application/json" in less than 1 seconds
    And at least one file in "/tmp/output" content match the following regex: "\"class_name\":\"(military uniform|bulletproof vest|suit|Windsor tie)\"" in less than 30 seconds
    And the Minifi logs do not contain errors
