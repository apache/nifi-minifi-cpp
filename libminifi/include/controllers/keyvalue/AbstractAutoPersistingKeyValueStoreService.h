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
#ifndef LIBMINIFI_INCLUDE_KEYVALUE_AbstractAutoPersistingKeyValueStoreService_H_
#define LIBMINIFI_INCLUDE_KEYVALUE_AbstractAutoPersistingKeyValueStoreService_H_

#include "PersistableKeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"

#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class AbstractAutoPersistingKeyValueStoreService : public PersistableKeyValueStoreService {
 public:
  explicit AbstractAutoPersistingKeyValueStoreService(const std::string& name, const std::string& id);
  explicit AbstractAutoPersistingKeyValueStoreService(const std::string& name, utils::Identifier uuid = utils::Identifier());

  virtual ~AbstractAutoPersistingKeyValueStoreService();

  static core::Property AlwaysPersist;
  static core::Property AutoPersistenceInterval;

  virtual void initialize() override;
  virtual void onEnable() override;
  virtual void notifyStop() override;

 protected:
  bool always_persist_;
  uint64_t auto_persistence_interval_;

  std::thread persisting_thread_;
  bool running_;
  std::mutex persisting_mutex_;
  std::condition_variable persisting_cv_;
  void persistingThreadFunc();

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_KEYVALUE_AbstractAutoPersistingKeyValueStoreService_H_ */
