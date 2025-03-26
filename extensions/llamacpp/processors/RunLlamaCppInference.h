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

#include "core/Processor.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinitionBuilder.h"
#include "LlamaContext.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

class RunLlamaCppInference : public core::ProcessorImpl {
  struct LLMExample {
    std::string input_role;
    std::string input;
    std::string output_role;
    std::string output;
  };

 public:
  explicit RunLlamaCppInference(std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
  }
  ~RunLlamaCppInference() override = default;

  EXTENSIONAPI static constexpr const char* Description = "LlamaCpp processor to use llama.cpp library for running langage model inference. "
      "The final prompt used for the inference created using the System Prompt and Prompt proprerty values and the content of the flowfile referred to as input data or flow file content.";

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
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("0")
      .build();
  EXTENSIONAPI static constexpr auto TextContextSize = core::PropertyDefinitionBuilder<>::createProperty("Text Context Size")
      .withDescription("Size of the text context, use 0 to use size set in model.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("4096")
      .build();
  EXTENSIONAPI static constexpr auto LogicalMaximumBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Logical Maximum Batch Size")
      .withDescription("Logical maximum batch size that can be submitted to the llama.cpp decode function.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("2048")
      .build();
  EXTENSIONAPI static constexpr auto PhysicalMaximumBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Physical Maximum Batch Size")
      .withDescription("Physical maximum batch size.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("512")
      .build();
  EXTENSIONAPI static constexpr auto MaxNumberOfSequences = core::PropertyDefinitionBuilder<>::createProperty("Max Number Of Sequences")
      .withDescription("Maximum number of sequences (i.e. distinct states for recurrent models).")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("1")
      .build();
  EXTENSIONAPI static constexpr auto ThreadsForGeneration = core::PropertyDefinitionBuilder<>::createProperty("Threads For Generation")
      .withDescription("Number of threads to use for generation.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("4")
      .build();
  EXTENSIONAPI static constexpr auto ThreadsForBatchProcessing = core::PropertyDefinitionBuilder<>::createProperty("Threads For Batch Processing")
      .withDescription("Number of threads to use for batch processing.")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::INTEGER_TYPE)
      .withDefaultValue("4")
      .build();
  EXTENSIONAPI static constexpr auto Prompt = core::PropertyDefinitionBuilder<>::createProperty("Prompt")
      .withDescription("The user prompt for the inference.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto SystemPrompt = core::PropertyDefinitionBuilder<>::createProperty("System Prompt")
      .withDescription("The system prompt for the inference.")
      .withDefaultValue("You are a helpful assisstant. You are given a question with some possible input data otherwise called flow file content. "
                        "You are expected to generate a response based on the quiestion and the input data.")
      .isRequired(true)
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

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = true;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void notifyStop() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RunLlamaCppInference>::getLogger(uuid_);

  std::string model_path_;
  std::vector<LLMExample> examples_;
  std::string system_prompt_;

  std::unique_ptr<LlamaContext> llama_ctx_;
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
