// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <array>
#include <string_view>
#include <utility>

#include "core/ProcessorImpl.h"
#include "agent/agent_docs.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::test {

class DummyProcessor : public minifi::core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  explicit DummyProcessor(std::string_view name)
    : ProcessorImpl{minifi::core::ProcessorMetadata{
      .uuid = minifi::utils::IdGenerator::getIdGenerator()->generate(),
      .name = std::string{name},
      .logger = minifi::core::logging::LoggerFactory<DummyProcessor>::getLogger()
    }} {}

  static constexpr const char* Description = "A processor that does nothing.";
  static constexpr auto SimpleProperty = core::PropertyDefinitionBuilder<>::createProperty("Simple Property")
      .withDescription("Just a simple string property")
      .build();
  static constexpr auto SensitiveProperty = core::PropertyDefinitionBuilder<>::createProperty("Sensitive Property")
      .withDescription("Sensitive property")
      .isSensitive(true)
      .build();
  static constexpr auto Properties = std::array<core::PropertyReference, 2>{SimpleProperty, SensitiveProperty};
  static constexpr core::RelationshipDefinition Success{"success", "Success relationship"};
  static constexpr auto Relationships = std::array{Success};
  static constexpr bool SupportsDynamicProperties = true;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override { setSupportedProperties(Properties); }
};

}  // namespace org::apache::nifi::minifi::test
