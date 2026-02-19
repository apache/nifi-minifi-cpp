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
#include "RunLlamaCppInference.h"
#include "api/core/Resource.h"
#include "minifi-cpp/core/FlowFile.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestBase.h"
#include "utils/CProcessor.h"
#include "unit/TestUtils.h"
#include "CProcessorTestUtils.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::test {

class MockLlamaContext : public processors::LlamaContext {
 public:
  std::optional<std::string> applyTemplate(const std::vector<processors::LlamaChatMessage>& messages) override {
    if (fail_apply_template_) {
      return std::nullopt;
    }
    messages_ = messages;
    return "Test input";
  }

  nonstd::expected<processors::GenerationResult, std::string> generate(const std::string& input, std::function<void(std::string_view/*token*/)> token_handler) override {
    if (fail_generation_) {
      return nonstd::make_unexpected("Generation failed");
    }
    processors::GenerationResult result;
    input_ = input;
    token_handler("Test ");
    token_handler("generated");
    token_handler(" content");
    result.time_to_first_token = std::chrono::milliseconds(100);
    result.num_tokens_in = 10;
    result.num_tokens_out = 3;
    result.tokens_per_second = 2.0;
    return result;
  }

  [[nodiscard]] const std::vector<processors::LlamaChatMessage>& getMessages() const {
    return messages_;
  }

  [[nodiscard]] const std::string& getInput() const {
    return input_;
  }

  void setGenerationFailure() {
    fail_generation_ = true;
  }

  void setApplyTemplateFailure() {
    fail_apply_template_ = true;
  }

 private:
  bool fail_generation_{false};
  bool fail_apply_template_{false};
  std::vector<processors::LlamaChatMessage> messages_;
  std::string input_;
};

TEST_CASE("Prompt is generated correctly with default parameters") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  std::filesystem::path test_model_path;
  processors::LlamaSamplerParams test_sampler_params;
  processors::LlamaContextParams test_context_params;
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path& model_path, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams& context_params) {
      test_model_path = model_path;
      test_sampler_params = sampler_params;
      test_context_params = context_params;
      return std::move(mock_llama_context);
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "Dummy model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));

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
  CHECK(*output_flow_file->getAttribute(processors::RunLlamaCppInference::LlamaCppTimeToFirstToken.name) == "100 ms");
  CHECK(*output_flow_file->getAttribute(processors::RunLlamaCppInference::LlamaCppTokensPerSecond.name) == "2.00");
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  REQUIRE(mock_llama_context_ptr->getMessages().size() == 2);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "system");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "You are a helpful assistant. You are given a question with some possible input data otherwise called flow file content. "
                                                            "You are expected to generate a response based on the question and the input data.");
  CHECK(mock_llama_context_ptr->getMessages()[1].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[1].content == "Input data (or flow file content):\n42\n\nQuestion: What is the answer to life, the universe and everything?");
}

TEST_CASE("Prompt is generated correctly with custom parameters") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  std::filesystem::path test_model_path;
  processors::LlamaSamplerParams test_sampler_params;
  processors::LlamaContextParams test_context_params;
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path& model_path, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams& context_params) {
      test_model_path = model_path;
      test_sampler_params = sampler_params;
      test_context_params = context_params;
      return std::move(mock_llama_context);
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "/path/to/model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Temperature.name, "0.4"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopK.name, "20"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopP.name, ""));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MinP.name, "0.1"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MinKeep.name, "1"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TextContextSize.name, "4096"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::LogicalMaximumBatchSize.name, "1024"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::PhysicalMaximumBatchSize.name, "796"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MaxNumberOfSequences.name, "2"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ThreadsForGeneration.name, "12"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ThreadsForBatchProcessing.name, "8"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::SystemPrompt.name, "Whatever"));

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
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::move(mock_llama_context);
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "Dummy model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "", .attributes = {}});

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Success)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  REQUIRE(mock_llama_context_ptr->getMessages().size() == 2);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "system");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "You are a helpful assistant. You are given a question with some possible input data otherwise called flow file content. "
                                                            "You are expected to generate a response based on the question and the input data.");
  CHECK(mock_llama_context_ptr->getMessages()[1].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[1].content == "Question: What is the answer to life, the universe and everything?");
}

TEST_CASE("Invalid values for optional double type properties throw exception") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::make_unique<MockLlamaContext>();
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "Dummy model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));

  std::string property_name;
  SECTION("Invalid value for Temperature property") {
    REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Temperature.name, "invalid_value"));
    property_name = processors::RunLlamaCppInference::Temperature.name;
  }
  SECTION("Invalid value for Top P property") {
    REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopP.name, "invalid_value"));
    property_name = processors::RunLlamaCppInference::TopP.name;
  }
  SECTION("Invalid value for Min P property") {
    REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::MinP.name, "invalid_value"));
    property_name = processors::RunLlamaCppInference::MinP.name;
  }

  REQUIRE_THROWS(controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}}));
  CHECK(minifi::test::utils::verifyLogLinePresenceInPollTime(1s,
      fmt::format("Expected parsable float from \"{}\", but got GeneralParsingError (Parsing Error:0)", property_name)));
}

TEST_CASE("Top K property empty and invalid values are handled properly") {
  std::optional<int32_t> test_top_k = 0;
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams& sampler_params, const processors::LlamaContextParams&) {
      test_top_k = sampler_params.top_k;
      return std::make_unique<MockLlamaContext>();
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "Dummy model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));
  SECTION("Empty value for Top K property") {
    REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopK.name, ""));
    auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
    REQUIRE(test_top_k == std::nullopt);
  }
  SECTION("Invalid value for Top K property") {
    REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::TopK.name, "invalid_value"));
    REQUIRE_THROWS(controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}}));
    CHECK(minifi::test::utils::verifyLogLinePresenceInPollTime(1s,
                        "Expected parsable int64_t from \"Top K\", but got GeneralParsingError (Parsing Error:0)"));
  }
}

TEST_CASE("Error handling during generation and applying template") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();

  SECTION("Generation fails with error") {
    mock_llama_context->setGenerationFailure();
  }

  SECTION("Applying template fails with error") {
    mock_llama_context->setApplyTemplateFailure();
  }

  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::move(mock_llama_context);
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "/path/to/model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).empty());
  REQUIRE(results.at(processors::RunLlamaCppInference::Failure).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Failure)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "42");
}

TEST_CASE("Route flow file to failure when prompt and input data is empty") {
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::make_unique<MockLlamaContext>();
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "/path/to/model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, ""));

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "", .attributes = {}});

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).empty());
  REQUIRE(results.at(processors::RunLlamaCppInference::Failure).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Failure)[0];
  CHECK(controller.plan->getContent(output_flow_file).empty());
}

TEST_CASE("System prompt is optional") {
  auto mock_llama_context = std::make_unique<MockLlamaContext>();
  auto mock_llama_context_ptr = mock_llama_context.get();
  minifi::test::SingleProcessorTestController controller(minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::move(mock_llama_context);
    }));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "Dummy model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::SystemPrompt.name, ""));

  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).size() == 1);
  auto& output_flow_file = results.at(processors::RunLlamaCppInference::Success)[0];
  CHECK(controller.plan->getContent(output_flow_file) == "Test generated content");
  CHECK(mock_llama_context_ptr->getInput() == "Test input");
  REQUIRE(mock_llama_context_ptr->getMessages().size() == 1);
  CHECK(mock_llama_context_ptr->getMessages()[0].role == "user");
  CHECK(mock_llama_context_ptr->getMessages()[0].content == "Input data (or flow file content):\n42\n\nQuestion: What is the answer to life, the universe and everything?");
}

TEST_CASE("Test output metrics") {
  auto processor = minifi::test::utils::make_custom_c_processor<processors::RunLlamaCppInference>(
    core::ProcessorMetadata{utils::Identifier{}, "RunLlamaCppInference", logging::LoggerFactory<processors::RunLlamaCppInference>::getLogger()},
    [&](const std::filesystem::path&, const processors::LlamaSamplerParams&, const processors::LlamaContextParams&) {
      return std::make_unique<MockLlamaContext>();
    });
  auto processor_metrics = processor->getMetrics();
  minifi::test::SingleProcessorTestController controller(std::move(processor));
  LogTestController::getInstance().setTrace<processors::RunLlamaCppInference>();
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::ModelPath.name, "Dummy model"));
  REQUIRE(controller.getProcessor()->setProperty(processors::RunLlamaCppInference::Prompt.name, "Question: What is the answer to life, the universe and everything?"));

  controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});
  auto results = controller.trigger(minifi::test::InputFlowFileData{.content = "42", .attributes = {}});

  REQUIRE(results.at(processors::RunLlamaCppInference::Success).size() == 1);
  auto prometheus_metrics = processor_metrics->calculateMetrics();
  REQUIRE(prometheus_metrics.size() >= 2);
  CHECK(prometheus_metrics[prometheus_metrics.size() - 2].name == "tokens_in");
  CHECK(prometheus_metrics[prometheus_metrics.size() - 2].value == 20);
  CHECK(prometheus_metrics[prometheus_metrics.size() - 1].name == "tokens_out");
  CHECK(prometheus_metrics[prometheus_metrics.size() - 1].value == 6);
  auto c2_metrics = processor_metrics->serialize();
  REQUIRE_FALSE(c2_metrics.empty());
  REQUIRE(c2_metrics[0].children.size() >= 2);
  CHECK(c2_metrics[0].children[c2_metrics[0].children.size() - 2].name == "TokensIn");
  CHECK(c2_metrics[0].children[c2_metrics[0].children.size() - 2].value.to_string() == "20");
  CHECK(c2_metrics[0].children[c2_metrics[0].children.size() - 1].name == "TokensOut");
  CHECK(c2_metrics[0].children[c2_metrics[0].children.size() - 1].value.to_string() == "6");
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::test
