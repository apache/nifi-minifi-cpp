/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <string>

#include <tensorflow/cc/framework/scope.h>
#include <tensorflow/cc/ops/standard_ops.h>
#include <processors/PutFile.h>
#include <processors/GetFile.h>
#include <processors/LogAttribute.h>
#include <TFConvertImageToTensor.h>
#include <TFExtractTopLabels.h>
#include "TFApplyGraph.h"

#define CATCH_CONFIG_MAIN

#include "../TestBase.h"

TEST_CASE("TensorFlow: Apply Graph", "[tfApplyGraph]") { // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TFApplyGraph>();
  LogTestController::getInstance().setTrace<processors::PutFile>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for input protocol buffers
  std::string in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&in_dir[0]) != nullptr);

  // Define input graph protocol buffer file
  std::string in_graph_file(in_dir);
  in_graph_file.append("/in_graph.pb");

  // Define input tensor protocol buffer file
  std::string in_tensor_file(in_dir);
  in_tensor_file.append("/tensor.pb");

  // Define directory for output protocol buffers
  std::string out_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&out_dir[0]) != nullptr);

  // Define output tensor protocol buffer file
  std::string out_tensor_file(out_dir);
  out_tensor_file.append("/tensor.pb");

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "Get Proto");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), in_dir);
  plan->setProperty(
      get_file,
      processors::GetFile::KeepSourceFile.getName(),
      "false");
  plan->addProcessor(
      "LogAttribute",
      "Log Pre Graph Apply",
      core::Relationship("success", "description"),
      true);
  auto tf_apply = plan->addProcessor(
      "TFApplyGraph",
      "Apply Graph",
      core::Relationship("success", "description"),
      true);
  plan->addProcessor(
      "LogAttribute",
      "Log Post Graph Apply",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      tf_apply,
      processors::TFApplyGraph::InputNode.getName(),
      "Input");
  plan->setProperty(
      tf_apply,
      processors::TFApplyGraph::OutputNode.getName(),
      "Output");
  auto put_file = plan->addProcessor(
      "PutFile",
      "Put Output Tensor",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      put_file,
      processors::PutFile::Directory.getName(),
      out_dir);
  plan->setProperty(
      put_file,
      processors::PutFile::ConflictResolution.getName(),
      processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);

  // Build test TensorFlow graph
  {
    tensorflow::Scope root = tensorflow::Scope::NewRootScope();
    auto d = tensorflow::ops::Placeholder(root.WithOpName("Input"), tensorflow::DT_FLOAT);
    auto v = tensorflow::ops::Add(root.WithOpName("Output"), d, d);
    tensorflow::GraphDef graph;

    // Write test TensorFlow graph
    root.ToGraphDef(&graph);
    std::ofstream in_file_stream(in_graph_file);
    graph.SerializeToOstream(&in_file_stream);
  }

  // Read test TensorFlow graph into TFApplyGraph
  plan->runNextProcessor([&get_file, &in_graph_file, &plan](const std::shared_ptr<core::ProcessContext> context,
                                                            const std::shared_ptr<core::ProcessSession> session) {
    // Intercept the call so that we can add an attr (won't be required when we have UpdateAttribute processor)
    auto flow_file = session->create();
    session->import(in_graph_file, flow_file, false);
    flow_file->addAttribute("tf.type", "graph");
    session->transfer(flow_file, processors::GetFile::Success);
    session->commit();
  });

  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // ApplyGraph (loads graph)

  // Write test input tensor
  {
    tensorflow::Tensor input(tensorflow::DT_FLOAT, {1, 1});
    input.flat<float>().data()[0] = 2.0f;
    tensorflow::TensorProto tensor_proto;
    input.AsProtoTensorContent(&tensor_proto);

    std::ofstream in_file_stream(in_tensor_file);
    tensor_proto.SerializeToOstream(&in_file_stream);
  }

  plan->reset();
  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // ApplyGraph (applies graph)
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // PutFile

  // Read test output tensor
  {
    std::ifstream out_file_stream(out_tensor_file);
    tensorflow::TensorProto tensor_proto;
    tensor_proto.ParseFromIstream(&out_file_stream);
    tensorflow::Tensor tensor;
    tensor.FromProto(tensor_proto);

    // Verify output tensor
    float tensor_val = tensor.flat<float>().data()[0];
    REQUIRE(tensor_val == 4.0f);
  }
}

TEST_CASE("TensorFlow: ConvertImageToTensor", "[tfConvertImageToTensor]") { // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TFConvertImageToTensor>();
  LogTestController::getInstance().setTrace<processors::PutFile>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for input protocol buffers
  std::string in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&in_dir[0]) != nullptr);

  // Define input tensor protocol buffer file
  std::string in_img_file(in_dir);
  in_img_file.append("/img");

  // Define directory for output protocol buffers
  std::string out_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&out_dir[0]) != nullptr);

  // Define output tensor protocol buffer file
  std::string out_tensor_file(out_dir);
  out_tensor_file.append("/img");

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "Get Proto");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), in_dir);
  plan->setProperty(
      get_file,
      processors::GetFile::KeepSourceFile.getName(),
      "false");
  plan->addProcessor(
      "LogAttribute",
      "Log Pre Graph Apply",
      core::Relationship("success", "description"),
      true);
  auto tf_apply = plan->addProcessor(
      "TFConvertImageToTensor",
      "Convert Image",
      core::Relationship("success", "description"),
      true);
  plan->addProcessor(
      "LogAttribute",
      "Log Post Graph Apply",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      tf_apply,
      processors::TFConvertImageToTensor::ImageFormat.getName(),
      "RAW");
  plan->setProperty(
      tf_apply,
      processors::TFConvertImageToTensor::InputWidth.getName(),
      "2");
  plan->setProperty(
      tf_apply,
      processors::TFConvertImageToTensor::InputHeight.getName(),
      "2");
  plan->setProperty(
      tf_apply,
      processors::TFConvertImageToTensor::OutputWidth.getName(),
      "10");
  plan->setProperty(
      tf_apply,
      processors::TFConvertImageToTensor::OutputHeight.getName(),
      "10");
  plan->setProperty(
      tf_apply,
      processors::TFConvertImageToTensor::NumChannels.getName(),
      "1");
  auto put_file = plan->addProcessor(
      "PutFile",
      "Put Output Tensor",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      put_file,
      processors::PutFile::Directory.getName(),
      out_dir);
  plan->setProperty(
      put_file,
      processors::PutFile::ConflictResolution.getName(),
      processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);

  // Write test input image
  {
    // 2x2 single-channel 8 bit per channel
    const uint8_t in_img_raw[2 * 2] = {0, 0,
                                       0, 0};

    std::ofstream in_file_stream(in_img_file);
    in_file_stream << in_img_raw;
  }

  plan->reset();
  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // TFConvertImageToTensor
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // PutFile

  // Read test output tensor
  {
    std::ifstream out_file_stream(out_tensor_file);
    tensorflow::TensorProto tensor_proto;
    tensor_proto.ParseFromIstream(&out_file_stream);
    tensorflow::Tensor tensor;
    tensor.FromProto(tensor_proto);

    // Verify output tensor
    auto shape = tensor.shape();
    auto shapeString = shape.DebugString();

    // Ensure output tensor is of the expected shape
    REQUIRE(shape.IsSameSize({1,     // Batch size
                              10,    // Width
                              10,    // Height
                              1}));  // Channels
  }
}

TEST_CASE("TensorFlow: Extract Top Labels", "[tfExtractTopLabels]") { // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::TFExtractTopLabels>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Define directory for input protocol buffers
  std::string in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&in_dir[0]) != nullptr);

  // Define input labels file
  std::string in_labels_file(in_dir);
  in_labels_file.append("/in_labels");

  // Define input tensor protocol buffer file
  std::string in_tensor_file(in_dir);
  in_tensor_file.append("/tensor.pb");

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "Get Input");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), in_dir);
  plan->setProperty(
      get_file,
      processors::GetFile::KeepSourceFile.getName(),
      "false");
  plan->addProcessor(
      "LogAttribute",
      "Log Pre Extract",
      core::Relationship("success", "description"),
      true);
  auto tf_apply = plan->addProcessor(
      "TFExtractTopLabels",
      "Extract",
      core::Relationship("success", "description"),
      true);
  plan->addProcessor(
      "LogAttribute",
      "Log Post Extract",
      core::Relationship("success", "description"),
      true);

  // Build test labels
  {
    // Write labels
    std::ofstream in_file_stream(in_labels_file);
    in_file_stream << "label_a\nlabel_b\nlabel_c\nlabel_d\nlabel_e\nlabel_f\nlabel_g\nlabel_h\nlabel_i\nlabel_j\n";
  }

  // Read labels
  plan->runNextProcessor([&get_file, &in_labels_file, &plan](const std::shared_ptr<core::ProcessContext> context,
                                                             const std::shared_ptr<core::ProcessSession> session) {
    // Intercept the call so that we can add an attr (won't be required when we have UpdateAttribute processor)
    auto flow_file = session->create();
    session->import(in_labels_file, flow_file, false);
    flow_file->addAttribute("tf.type", "labels");
    session->transfer(flow_file, processors::GetFile::Success);
    session->commit();
  });

  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // Extract (loads labels)

  // Write input tensor
  {
    tensorflow::Tensor input(tensorflow::DT_FLOAT, {10});
    input.flat<float>().data()[0] = 0.000f;
    input.flat<float>().data()[1] = 0.400f;
    input.flat<float>().data()[2] = 0.100f;
    input.flat<float>().data()[3] = 0.005f;
    input.flat<float>().data()[4] = 1.000f;
    input.flat<float>().data()[5] = 0.500f;
    input.flat<float>().data()[6] = 0.200f;
    input.flat<float>().data()[7] = 0.000f;
    input.flat<float>().data()[8] = 0.300f;
    input.flat<float>().data()[9] = 0.000f;
    tensorflow::TensorProto tensor_proto;
    input.AsProtoTensorContent(&tensor_proto);

    std::ofstream in_file_stream(in_tensor_file);
    tensor_proto.SerializeToOstream(&in_file_stream);
  }

  plan->reset();
  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // Extract
  plan->runNextProcessor();  // Log

  // Verify labels
  REQUIRE(LogTestController::getInstance().contains("key:tf.top_label_0 value:label_e"));
  REQUIRE(LogTestController::getInstance().contains("key:tf.top_label_1 value:label_f"));
  REQUIRE(LogTestController::getInstance().contains("key:tf.top_label_2 value:label_b"));
  REQUIRE(LogTestController::getInstance().contains("key:tf.top_label_3 value:label_i"));
  REQUIRE(LogTestController::getInstance().contains("key:tf.top_label_4 value:label_g"));
}
