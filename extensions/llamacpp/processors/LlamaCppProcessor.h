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

namespace org::apache::nifi::minifi::processors {

class LlamaCppProcessor : public core::Processor {
    static constexpr const char* DEFAULT_SYSTEM_PROMPT = R"(You are a helpful assistant or otherwise called an AI processor.
You are part of a flow based pipeline helping the user transforming and routing data (encapsulated in what is called flowfiles).
The user will provide the data, it will have attributes (name and value) and a content.
The output route is also called a relationship.
You should only output the transformed flowfiles and a relationships to be transferred to.
You might produce multiple flowfiles if instructed.
You get $10000 if you respond according to the expected format.
Do not use any other relationship than what the specified ones.
Only split flow files when it is explicitly requested.
Do not add extra attributes only when it is requested.

What now follows is a description of how the user would like you to transform/route their data, and what relationships you are allowed to use:
)";

  struct LLMExample {
    std::string input;
    std::string output;
  };



 public:
  explicit LlamaCppProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  ~LlamaCppProcessor() override = default;

  EXTENSIONAPI static constexpr const char* Description = "LlamaCpp processor";

  EXTENSIONAPI static constexpr auto ModelName = core::PropertyDefinitionBuilder<>::createProperty("Model Name")
      .withDescription("The name of the model")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Temperature = core::PropertyDefinitionBuilder<>::createProperty("Temperature")
      .withDescription("The inference temperature")
      .isRequired(true)
      .withDefaultValue("0.8")
      .build();
  EXTENSIONAPI static constexpr auto SystemPrompt = core::PropertyDefinitionBuilder<>::createProperty("System Prompt")
      .withDescription("The setup system prompt for the model")
      .isRequired(true)
      .withDefaultValue(DEFAULT_SYSTEM_PROMPT)
      .build();
  EXTENSIONAPI static constexpr auto Prompt = core::PropertyDefinitionBuilder<>::createProperty("Prompt")
      .withDescription("The prompt for the model")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Examples = core::PropertyDefinitionBuilder<>::createProperty("Examples")
      .withDescription("Example input/outputs pairs for the ai model")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
                                                                                            ModelName,
                                                                                            Temperature,
                                                                                            SystemPrompt,
                                                                                            Prompt,
                                                                                            Examples,
                                                                                         });


  EXTENSIONAPI static constexpr auto Malformed = core::RelationshipDefinition{"malformed", "Malformed output that could not be parsed"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Malformed};

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
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LlamaCppProcessor>::getLogger(uuid_);

  double temperature_{0};
  std::string model_name_;
  std::string system_prompt_;
  std::string prompt_;
  std::string full_prompt_;
  std::vector<LLMExample> examples_;

  std::unique_ptr<llamacpp::LlamaContext> llama_ctx_;
};

}  // namespace org::apache::nifi::minifi::processors
