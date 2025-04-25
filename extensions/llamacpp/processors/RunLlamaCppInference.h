/**
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

#pragma once

#include <mutex>
#include <atomic>

#include "core/Processor.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinitionBuilder.h"
#include "LlamaContext.h"
#include "core/ProcessorMetrics.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

class RunLlamaCppInferenceMetrics : public core::ProcessorMetricsImpl {
 public:
  explicit RunLlamaCppInferenceMetrics(const core::Processor& source_processor)
  : core::ProcessorMetricsImpl(source_processor) {
  }

  std::vector<state::response::SerializedResponseNode> serialize() override {
    auto resp = core::ProcessorMetricsImpl::serialize();
    auto& root_node = resp[0];

    state::response::SerializedResponseNode tokens_in_node{"TokensIn", tokens_in.load()};
    root_node.children.push_back(tokens_in_node);

    state::response::SerializedResponseNode tokens_out_node{"TokensOut", tokens_out.load()};
    root_node.children.push_back(tokens_out_node);

    return resp;
  }

  std::vector<state::PublishedMetric> calculateMetrics() override {
    auto metrics = core::ProcessorMetricsImpl::calculateMetrics();
    metrics.push_back({"tokens_in", static_cast<double>(tokens_in.load()), getCommonLabels()});
    metrics.push_back({"tokens_out", static_cast<double>(tokens_out.load()), getCommonLabels()});
    return metrics;
  }

  std::mutex tokens_in_mutex_;
  std::mutex tokens_out_mutex_;
  std::atomic<uint64_t> tokens_in{0};
  std::atomic<uint64_t> tokens_out{0};
};

class RunLlamaCppInference : public core::ProcessorImpl {
 public:
  explicit RunLlamaCppInference(std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
    metrics_ = gsl::make_not_null(std::make_shared<RunLlamaCppInferenceMetrics>(*this));
  }
  ~RunLlamaCppInference() override = default;

  EXTENSIONAPI static constexpr const char* Description = "LlamaCpp processor to use llama.cpp library for running language model inference. "
      "The inference will be based on the System Prompt and the Prompt property values, together with the content of the incoming flow file. "
      "In the Prompt, the content of the incoming flow file can be referred to as 'the input data' or 'the flow file content'.";

  EXTENSIONAPI static constexpr auto ModelPath = core::PropertyDefinitionBuilder<>::createProperty("Model Path")
      .withDescription("The filesystem path of the model file in gguf format.")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Temperature = core::PropertyDefinitionBuilder<>::createProperty("Temperature")
      .withDescription("The temperature to use for sampling.")
      .withDefaultValue("0.8")
      .build();
  EXTENSIONAPI static constexpr auto TopK = core::PropertyDefinitionBuilder<>::createProperty("Top K")
      .withDescription("Limit the next token selection to the K most probable tokens. Set <= 0 value to use vocab size.")
      .withDefaultValue("40")
      .build();
  EXTENSIONAPI static constexpr auto TopP = core::PropertyDefinitionBuilder<>::createProperty("Top P")
      .withDescription("Limit the next token selection to a subset of tokens with a cumulative probability above a threshold P. 1.0 = disabled.")
      .withDefaultValue("0.9")
      .build();
  EXTENSIONAPI static constexpr auto MinP = core::PropertyDefinitionBuilder<>::createProperty("Min P")
      .withDescription("Sets a minimum base probability threshold for token selection. 0.0 = disabled.")
      .build();
  EXTENSIONAPI static constexpr auto MinKeep = core::PropertyDefinitionBuilder<>::createProperty("Min Keep")
      .withDescription("If greater than 0, force samplers to return N possible tokens at minimum.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto TextContextSize = core::PropertyDefinitionBuilder<>::createProperty("Text Context Size")
      .withDescription("Size of the text context, use 0 to use size set in model.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("4096")
      .build();
  EXTENSIONAPI static constexpr auto LogicalMaximumBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Logical Maximum Batch Size")
      .withDescription("Logical maximum batch size that can be submitted to the llama.cpp decode function.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("2048")
      .build();
  EXTENSIONAPI static constexpr auto PhysicalMaximumBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Physical Maximum Batch Size")
      .withDescription("Physical maximum batch size.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("512")
      .build();
  EXTENSIONAPI static constexpr auto MaxNumberOfSequences = core::PropertyDefinitionBuilder<>::createProperty("Max Number Of Sequences")
      .withDescription("Maximum number of sequences (i.e. distinct states for recurrent models).")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto ThreadsForGeneration = core::PropertyDefinitionBuilder<>::createProperty("Threads For Generation")
      .withDescription("Number of threads to use for generation.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::INTEGER_VALIDATOR)
      .withDefaultValue("4")
      .build();
  EXTENSIONAPI static constexpr auto ThreadsForBatchProcessing = core::PropertyDefinitionBuilder<>::createProperty("Threads For Batch Processing")
      .withDescription("Number of threads to use for batch processing.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::INTEGER_VALIDATOR)
      .withDefaultValue("4")
      .build();
  EXTENSIONAPI static constexpr auto Prompt = core::PropertyDefinitionBuilder<>::createProperty("Prompt")
      .withDescription("The user prompt for the inference.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SystemPrompt = core::PropertyDefinitionBuilder<>::createProperty("System Prompt")
      .withDescription("The system prompt for the inference.")
      .withDefaultValue("You are a helpful assistant. You are given a question with some possible input data otherwise called flow file content. "
                        "You are expected to generate a response based on the question and the input data.")
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    ModelPath,
    Temperature,
    TopK,
    TopP,
    MinP,
    MinKeep,
    TextContextSize,
    LogicalMaximumBatchSize,
    PhysicalMaximumBatchSize,
    MaxNumberOfSequences,
    ThreadsForGeneration,
    ThreadsForBatchProcessing,
    Prompt,
    SystemPrompt
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "Generated results from the model"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "Generation failed"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto LlamaCppTimeToFirstToken = core::OutputAttributeDefinition<>{"llamacpp.time.to.first.token", {Success}, "Time to first token generated in milliseconds."};
  EXTENSIONAPI static constexpr auto LlamaCppTokensPerSecond = core::OutputAttributeDefinition<>{"llamacpp.tokens.per.second", {Success}, "Tokens generated per second."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 2>{LlamaCppTimeToFirstToken, LlamaCppTokensPerSecond};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void notifyStop() override;

 private:
  void increaseTokensIn(uint64_t token_count);
  void increaseTokensOut(uint64_t token_count);
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RunLlamaCppInference>::getLogger(uuid_);

  std::string model_path_;
  std::string system_prompt_;

  std::unique_ptr<LlamaContext> llama_ctx_;
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
