/**
 * @file Repository
 * Repository class declaration
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

#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minifi-cpp/ResourceClaim.h"
#include "Connectable.h"
#include "ContentRepository.h"
#include "Property.h"
#include "SerializableComponent.h"
#include "core/logging/LoggerFactory.h"
#include "RepositoryMetricsSource.h"
#include "minifi-cpp/properties/Configure.h"
#include "utils/BackTrace.h"
#include "minifi-cpp/SwapManager.h"
#include "core/Core.h"

#ifndef WIN32
#include <sys/stat.h>
#endif

namespace org::apache::nifi::minifi::core {

class Repository : public virtual core::CoreComponent, public virtual core::RepositoryMetricsSource {
 public:
  virtual bool initialize(const std::shared_ptr<Configure> &configure) = 0;
  virtual bool start() = 0;
  virtual bool stop() = 0;
  virtual bool isNoop() const = 0;
  virtual void flush() = 0;

  virtual bool Put(const std::string& /*key*/, const uint8_t* /*buf*/, size_t /*bufLen*/) = 0;
  virtual bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>>& /*data*/) = 0;
  virtual bool Delete(const std::string& /*key*/) = 0;
  virtual bool Delete(const std::shared_ptr<core::CoreComponent>& item) = 0;
  virtual bool Delete(std::vector<std::shared_ptr<core::SerializableComponent>> &storedValues) = 0;

  virtual void setConnectionMap(std::map<std::string, core::Connectable*> connectionMap) = 0;

  virtual void setContainers(std::map<std::string, core::Connectable*> containers) = 0;

  virtual bool Get(const std::string& /*key*/, std::string& /*value*/) = 0;

  virtual bool getElements(std::vector<std::shared_ptr<core::SerializableComponent>>& /*store*/, size_t& /*max_size*/) = 0;

  virtual bool storeElement(const std::shared_ptr<core::SerializableComponent>& element) = 0;

  virtual void loadComponent(const std::shared_ptr<core::ContentRepository>& /*content_repo*/) = 0;

  virtual std::string getDirectory() const = 0;
};

}  // namespace org::apache::nifi::minifi::core
