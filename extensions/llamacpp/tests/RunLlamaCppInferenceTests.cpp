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
#include "RunLlamaCppInference.h"
#include "unit/SingleProcessorTestController.h"
#include "core/FlowFile.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::test {

class MockLlamaContext : public processors::LlamaContext {
 public:
  std::string applyTemplate(const std::vector<processors::LlamaChatMessage>& messages) override {
    messages_ = messages;
    return "Test input";
  }

  nonstd::expected<uint64_t, std::string> generate(const std::string& input, std::function<void(std::string_view/*token*/)> token_handler) override {
    if (fail_generation_) {
      return nonstd::make_unexpected("Generation failed");
    }
    input_ = input;
    token_handler("Test ");
    token_handler("generated");
    token_handler(" content");
    return 3;
  }

  [[nodiscard]] const std::vector<processors::LlamaChatMessage>& getMessages() const {
    return messages_;
  }

  [[nodiscard]] const std::string& getInput() const {
    return input_;
  }

  void setFailure() {
    fail_generation_ = true;
  }

 private:
  bool fail_generation_{false};
  std::vector<processors::LlamaChatMessage> messages_;
  std::string input_;
};

TEST_CASE("Prompt is generated correctly with default parameters") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  std::filesystem::path test_model_path;
  processors::LlamaSamplerParams test_sampler_params;
  processors::LlamaContextParams test_context_params;
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path& model_path, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams& context_params) {
      test_model_path = model_path;
      test_sampler_params = sampler_params;
      test_context_params = context_params;
      return std::move(mock_llama_context);
    });
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::RunLlamaCppInference>("RunLlamaCppInference"));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath, "Dummy model");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt, "Question: What is the answer to life, the universe and everything?");

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
  CHECK(test_model_path == "Dummy model");
  CHECK(test_sampler_params.temperature == 0.8F);
  CHECK(test_sampler_params.top_k == 40);
  CHECK(test_sampler_params.top_p == 0.9F);
  CHECK(test_sampler_params.min_p == std::nullopt);
  CHECK(test_sampler_params.min_keep == 0);
  CHECK(test_context_params.n_ctx == 4096);
  CHECK(test_context_params.n_batch == 2048);
  CHECK(test_context_params.n_ubatch == 512);
  CHECK(test_context_params.n_seq_max == 1);
  CHECK(test_context_params.n_threads == 4);
  CHECK(test_context_params.n_threads_batch == 4);

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Success)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  REQUIRE(mock_llama_context_ptr->getMessages().size() == 2);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "system");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "You are a helpful assisstant. You are given a question with some possible input data otherwise called flow file content. "
                                                            "You are expected to generate a response based on the quiestion and the input data.");
  CHECK(mock_llama_context_ptr->getMessages()[1].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[1].content == "Input data (or flow file content):\n42\n\nQuestion: What is the answer to life, the universe and everything?");
}

TEST_CASE("Prompt is generated correctly with custom parameters") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  std::filesystem::path test_model_path;
  processors::LlamaSamplerParams test_sampler_params;
  processors::LlamaContextParams test_context_params;
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path& model_path, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams& context_params) {
      test_model_path = model_path;
      test_sampler_params = sampler_params;
      test_context_params = context_params;
      return std::move(mock_llama_context);
    });
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::RunLlamaCppInference>("RunLlamaCppInference"));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath, "/path/to/model");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt, "Question: What is the answer to life, the universe and everything?");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Temperature, "0.4");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopK, "20");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopP, "");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MinP, "0.1");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MinKeep, "1");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TextContextSize, "4096");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::LogicalMaximumBatchSize, "1024");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::PhysicalMaximumBatchSize, "796");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MaxNumberOfSequences, "2");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ThreadsForGeneration, "12");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ThreadsForBatchProcessing, "8");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::SystemPrompt, "Whatever");

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
  CHECK(test_model_path == "/path/to/model");
  CHECK(test_sampler_params.temperature == 0.4F);
  CHECK(test_sampler_params.top_k == 20);
  CHECK(test_sampler_params.top_p == std::nullopt);
  CHECK(test_sampler_params.min_p == 0.1F);
  CHECK(test_sampler_params.min_keep == 1);
  CHECK(test_context_params.n_ctx == 4096);
  CHECK(test_context_params.n_batch == 1024);
  CHECK(test_context_params.n_ubatch == 796);
  CHECK(test_context_params.n_seq_max == 2);
  CHECK(test_context_params.n_threads == 12);
  CHECK(test_context_params.n_threads_batch == 8);

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Success)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  REQUIRE(mock_llama_context_ptr->getMessages().size() == 2);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "system");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "Whatever");
  CHECK(mock_llama_context_ptr->getMessages()[1].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[1].content == "Input data (or flow file content):\n42\n\nQuestion: What is the answer to life, the universe and everything?");
}

TEST_CASE("Empty flow file does not include input data in prompt") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::move(mock_llama_context);
    });
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::RunLlamaCppInference>("RunLlamaCppInference"));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath, "Dummy model");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt, "Question: What is the answer to life, the universe and everything?");

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "", .attributes = {}});

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Success)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  REQUIRE(mock_llama_context_ptr->getMessages().size() == 2);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "system");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "You are a helpful assisstant. You are given a question with some possible input data otherwise called flow file content. "
                                                            "You are expected to generate a response based on the quiestion and the input data.");
  CHECK(mock_llama_context_ptr->getMessages()[1].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[1].content == "Question: What is the answer to life, the universe and everything?");
}

TEST_CASE("Invalid values for optional double type properties throw exception") {
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::make_unique<MockLlamaContext>();
    });
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::RunLlamaCppInference>("RunLlamaCppInference"));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath, "Dummy model");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt, "Question: What is the answer to life, the universe and everything?");

  std::string property_name;
  SECTION("Invalid value for Temperature property") {
    controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Temperature, "invalid_value");
    property_name = processors::RunLlamaCppInference::Temperature.name;
  }
  SECTION("Invalid value for Top P property") {
    controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopP, "invalid_value");
    property_name = processors::RunLlamaCppInference::TopP.name;
  }
  SECTION("Invalid value for Min P property") {
    controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MinP, "invalid_value");
    property_name = processors::RunLlamaCppInference::MinP.name;
  }

  REQUIRE_THROWS_WITH(controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}}),
                      fmt::format("Process Schedule Operation: Property '{}' has invalid value 'invalid_value'", property_name));
}

TEST_CASE("Top K property empty and invalid values are handled properly") {
  std::optional<int32_t> test_top_k;
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams&) {
      test_top_k = sampler_params.top_k;
      return std::make_unique<MockLlamaContext>();
    });
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::RunLlamaCppInference>("RunLlamaCppInference"));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath, "Dummy model");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt, "Question: What is the answer to life, the universe and everything?");
  SECTION("Empty value for Top K property") {
    controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopK, "");
    auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
    REQUIRE(test_top_k == std::nullopt);
  }
  SECTION("Invalid value for Top K property") {
    controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopK, "invalid_value");
    REQUIRE_THROWS_WITH(controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}}), "Process Schedule Operation: Property 'Top K' has invalid value 'invalid_value'");
  }
}

TEST_CASE("Generation failed with error") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  mock_llama_context->setFailure();
  processors::LlamaContext::testSetProvider(
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::move(mock_llama_context);
    });
  minifi::test::SingleProcessorTestController controller(std::make_unique<processors::RunLlamaCppInference>("RunLlamaCppInference"));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath, "/path/to/model");
  controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt, "Question: What is the answer to life, the universe and everything?");

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).empty());
  REQUIRE(results.at(processors::RunLlamaCppInference::Failure).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Failure)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "42");
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::test
