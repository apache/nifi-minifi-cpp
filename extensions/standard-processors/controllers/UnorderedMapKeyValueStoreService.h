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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_CONTROLLERS_UNORDEREDMAPKEYVALUESTORESERVICE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_CONTROLLERS_UNORDEREDMAPKEYVALUESTORESERVICE_H_

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <utility>

#include "controllers/keyvalue/KeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"
#include "controllers/keyvalue/PersistableKeyValueStoreService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

/// Key-value store serice purely in RAM without disk usage
class UnorderedMapKeyValueStoreService : virtual public PersistableKeyValueStoreService {
 public:
  explicit UnorderedMapKeyValueStoreService(const std::string& name, const utils::Identifier& uuid = {});
  explicit UnorderedMapKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure>& configuration);

  ~UnorderedMapKeyValueStoreService() override;

  bool set(const std::string& key, const std::string& value) override;

  bool get(const std::string& key, std::string& value) override;

  bool get(std::unordered_map<std::string, std::string>& kvs) override;

  bool remove(const std::string& key) override;

  bool clear() override;

  bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) override;

  bool persist() override {
    return true;
  }

 protected:
  std::unordered_map<std::string, std::string> map_;
  std::recursive_mutex mutex_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_CONTROLLERS_UNORDEREDMAPKEYVALUESTORESERVICE_H_
