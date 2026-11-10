// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <string>
#include <string_view>
#include <type_traits>
#include "range/v3/view/transform.hpp"
#include "minifi-cpp/core/Annotation.h"
#include "core/Core.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "utils/StringUtils.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core {
template<typename ProcessorT>
class AbstractProcessor : public ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  void initialize() final {
    static_assert(std::is_same_v<typename decltype(ProcessorT::Properties)::value_type, PropertyReference>);
    static_assert(std::is_same_v<typename decltype(ProcessorT::Relationships)::value_type, RelationshipDefinition>);
    setSupportedProperties(ProcessorT::Properties);
    setSupportedRelationships(ProcessorT::Relationships);
  }

  void onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) override = 0;
  void onTrigger(core::ProcessContext&, core::ProcessSession&) override = 0;

  bool supportsDynamicProperties() const noexcept final { return ProcessorT::SupportsDynamicProperties; }
  bool supportsDynamicRelationships() const noexcept final { return ProcessorT::SupportsDynamicRelationships; }
  minifi::core::annotation::Input getInputRequirement() const noexcept final { return ProcessorT::InputRequirement; }
  bool isSingleThreaded() const noexcept final { return ProcessorT::IsSingleThreaded; }
};
}  // namespace org::apache::nifi::minifi::core
