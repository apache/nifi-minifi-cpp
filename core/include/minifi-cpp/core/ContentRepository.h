/**
 *
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
#include "ContentSession.h"
#include "minifi-cpp/core/RepositoryMetricsSource.h"
#include "core/Core.h"
#include "utils/GeneralUtils.h"

namespace org::apache::nifi::minifi::core {

/**
 * Content repository definition that extends StreamManager.
 */
class ContentRepository : public virtual core::CoreComponent, public virtual StreamManager<minifi::ResourceClaim>, public virtual utils::EnableSharedFromThis, public virtual core::RepositoryMetricsSource {
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
