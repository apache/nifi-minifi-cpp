// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <filesystem>

#include "core/ProcessGroup.h"
#include "utils/StringUtils.h"
#include "FlowStatusRequest.h"
#include "core/BulletinStore.h"

namespace org::apache::nifi::minifi::c2 {

class ControllerSocketReporter {
 public:
  struct QueueSize {
    uint32_t queue_size{};
    uint32_t queue_size_max{};
  };

  virtual std::unordered_map<std::string, QueueSize> getQueueSizes() = 0;
  virtual std::unordered_set<std::string> getFullConnections() = 0;
  virtual std::unordered_set<std::string> getConnections() = 0;
  virtual std::string getAgentManifest() = 0;
  virtual void setRoot(core::ProcessGroup* root) = 0;
  virtual void setFlowStatusDependencies(core::BulletinStore* bulletin_store, const std::filesystem::path& flowfile_repo_dir, const std::filesystem::path& content_repo_dir) = 0;
  virtual std::string getFlowStatus(const std::vector<FlowStatusRequest>& requests) = 0;
  virtual ~ControllerSocketReporter() = default;
};

}  // namespace org::apache::nifi::minifi::c2
