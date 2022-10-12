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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_KEYVALUESTORESERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_KEYVALUESTORESERVICE_H_

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>
#include <functional>

#include "core/Core.h"
#include "properties/Configure.h"
#include "core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::controllers {

class KeyValueStoreService : public core::controller::ControllerService {
 public:
  explicit KeyValueStoreService(std::string name, const utils::Identifier& uuid = {});

  ~KeyValueStoreService() override;

  void yield() override;
  bool isRunning() override;
  bool isWorkAvailable() override;

  virtual bool set(const std::string& key, const std::string& value) = 0;

  virtual bool get(const std::string& key, std::string& value) = 0;

  virtual bool get(std::unordered_map<std::string, std::string>& kvs) = 0;

  virtual bool remove(const std::string& key) = 0;

  virtual bool clear() = 0;

  virtual bool update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) = 0;
};

}  // namespace org::apache::nifi::minifi::controllers

#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_KEYVALUESTORESERVICE_H_
