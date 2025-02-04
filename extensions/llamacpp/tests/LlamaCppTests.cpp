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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "LlamaCppProcessor.h"
#include "unit/SingleProcessorTestController.h"
#include "core/FlowFile.h"

namespace minifi = org::apache::nifi::minifi;

class MockLlamaContext : public minifi::processors::llamacpp::LlamaContext {
 public:
  std::string applyTemplate(const std::vector<minifi::processors::llamacpp::LlamaChatMessage>& messages) override {
    return "Test Message";
  }
  void generate(const std::string& input, std::function<bool(std::string_view/*token*/)> cb) override {
    cb(
      "attributes:\n"
      "  a: 1\n"
      "  b: 2\n"
      "content:\n"
      "  Test content\n"
      "relationship:\n"
      "  banana\n"
      "attributes:\n"
      "messed up result\n"
    );
  }
  ~MockLlamaContext() override = default;
};

TEST_CASE("Output is correctly parsed and routed") {
  minifi::processors::llamacpp::LlamaContext::testSetProvider([] (const std::filesystem::path&, float) {return std::make_unique<MockLlamaContext>();});
  minifi::test::SingleProcessorTestController controller(std::make_unique<minifi::processors::LlamaCppProcessor>("LlamaCppProcessor"));
  controller.addDynamicRelationship("banana");
  controller.getProcessor()->setProperty(minifi::processors::LlamaCppProcessor::ModelName, "Dummy model");
  controller.getProcessor()->setProperty(minifi::processors::LlamaCppProcessor::Prompt, "Do whatever");
  controller.getProcessor()->setProperty(minifi::processors::LlamaCppProcessor::Examples, "[]");


  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "some data", .attributes = {}});
  CHECK(results.size() == 2);
  CHECK(results[core::Relationship{"malformed", ""}].size() == 1);
  auto outputs = results[core::Relationship{"banana", ""}];
  REQUIRE(outputs.size() == 1);
  CHECK(controller.plan->getContent(outputs[0]) == "Test content");
  CHECK(outputs[0]->getAttribute("a") == "1");
  CHECK(outputs[0]->getAttribute("b") == "2");
}
