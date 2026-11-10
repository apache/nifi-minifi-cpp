// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <list>

#include "minifi-cpp/properties/Configure.h"
#include "minifi-cpp/ResourceClaim.h"
#include "StreamManager.h"
#include "minifi-cpp/core/ContentSession.h"
#include "minifi-cpp/core/RepositoryMetricsSource.h"
#include "core/Core.h"
#include "utils/GeneralUtils.h"

namespace org::apache::nifi::minifi::core {

/**
 * Content repository definition that extends StreamManager.
 */
class ContentRepository : public virtual core::CoreComponent, public virtual StreamManager<minifi::ResourceClaim>,
    public virtual utils::EnableSharedFromThis, public virtual core::RepositoryMetricsSource {
 public:
  ~ContentRepository() override = default;

  virtual bool initialize(const std::shared_ptr<Configure> &configure) = 0;
  virtual std::shared_ptr<ContentSession> createSession() = 0;
  virtual void reset() = 0;
  virtual void clearOrphans() = 0;

  virtual void start() = 0;
  virtual void stop() = 0;
};

}  // namespace org::apache::nifi::minifi::core
