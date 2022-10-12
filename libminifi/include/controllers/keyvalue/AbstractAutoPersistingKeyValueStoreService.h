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

#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include <utility>

#include "PersistableKeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::controllers {

class AbstractAutoPersistingKeyValueStoreService : virtual public PersistableKeyValueStoreService {
 public:
  explicit AbstractAutoPersistingKeyValueStoreService(std::string name, const utils::Identifier& uuid = {});

  ~AbstractAutoPersistingKeyValueStoreService() override;

  static constexpr const char* AlwaysPersistPropertyName = "Always Persist";
  static constexpr const char* AutoPersistenceIntervalPropertyName = "Auto Persistence Interval";

  void onEnable() override;
  void notifyStop() override;

 protected:
  bool always_persist_;
  std::chrono::milliseconds auto_persistence_interval_;

  std::thread persisting_thread_;
  bool running_;
  std::mutex persisting_mutex_;
  std::condition_variable persisting_cv_;
  void persistingThreadFunc();

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AbstractAutoPersistingKeyValueStoreService>::getLogger();

  void stopPersistingThread();
};

}  // namespace org::apache::nifi::minifi::controllers
