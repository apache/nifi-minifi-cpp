// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <utility>

#include "ForwardingNode.h"

namespace org::apache::nifi::minifi {

class Funnel final : public ForwardingNode {
 public:
  Funnel(std::string_view name, const utils::Identifier& uuid)
      : ForwardingNode(core::ProcessorMetadata{.uuid = uuid, .name = std::string{name}, .logger = core::logging::LoggerFactory<Funnel>::getLogger(uuid)}) {}

  MINIFIAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext&, core::ProcessSessionFactory&) override;
};

}  // namespace org::apache::nifi::minifi
