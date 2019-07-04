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
#ifndef LIBMINIFI_INCLUDE_KEYVALUE_KeyValueStoreService_H_
#define LIBMINIFI_INCLUDE_KEYVALUE_KeyValueStoreService_H_

#include "core/Core.h"
#include "properties/Configure.h"
#include "core/controller/ControllerService.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>
#include <functional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class KeyValueStoreService : public core::controller::ControllerService {
 public:
  explicit KeyValueStoreService(const std::string& name, const std::string& id);
  explicit KeyValueStoreService(const std::string& name, utils::Identifier uuid = utils::Identifier());

  virtual ~KeyValueStoreService();

  virtual void yield() override;
  virtual bool isRunning() override;
  virtual bool isWorkAvailable() override;

  virtual bool set(const std::string& key, const std::string& value) = 0;

  virtual bool get(const std::string& key, std::string& value) = 0;

  virtual bool get(std::unordered_map<std::string, std::string>& kvs) = 0;

  virtual bool remove(const std::string& key) = 0;

  virtual bool clear() = 0;

  virtual bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) = 0;
};

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_KEYVALUE_KeyValueStoreService_H_ */
