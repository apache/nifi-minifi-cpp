// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <future>
#include <vector>
#include <memory>

#include "minifi-cpp/core/FlowFile.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi {

struct SwappedFlowFile {
  utils::Identifier id;
  std::chrono::steady_clock::time_point to_be_processed_after;
};

class SwapManager {
 public:
  virtual void store(std::vector<std::shared_ptr<core::FlowFile>> flow_files) = 0;
  virtual std::future<std::vector<std::shared_ptr<core::FlowFile>>> load(std::vector<SwappedFlowFile> flow_files) = 0;
  virtual ~SwapManager() = default;
};

}  // namespace org::apache::nifi::minifi
