// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <variant>

#include "core/Core.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::extensions::script {

class ScriptExecutor : public minifi::core::CoreComponentImpl {
 public:
  ScriptExecutor(const std::string_view name, const utils::Identifier& uuid) : core::CoreComponentImpl(name, uuid) {}

  virtual void onTrigger(core::ProcessContext& context, core::ProcessSession& session) = 0;
  virtual void initialize(std::filesystem::path script_file,
      std::string script_body,
      std::optional<std::string> module_directory,
      size_t max_concurrent_engines,
      const core::Relationship& success,
      const core::Relationship& failure,
      const core::Relationship& original,
      const std::shared_ptr<core::logging::Logger>& logger) = 0;

 protected:
  std::variant<std::monostate, std::filesystem::path, std::string> script_to_run_;
  std::optional<std::string> module_directory_;
};
}  // namespace org::apache::nifi::minifi::extensions::script
