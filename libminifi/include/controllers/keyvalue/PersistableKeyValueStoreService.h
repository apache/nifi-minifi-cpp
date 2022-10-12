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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_PERSISTABLEKEYVALUESTORESERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_PERSISTABLEKEYVALUESTORESERVICE_H_

#include <string>
#include <unordered_map>
#include <map>

#include "KeyValueStoreService.h"
#include "AbstractCoreComponentStateManagerProvider.h"
#include "core/Core.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::controllers {

class PersistableKeyValueStoreService : public KeyValueStoreService, public AbstractCoreComponentStateManagerProvider {
 public:
  explicit PersistableKeyValueStoreService(std::string name, const utils::Identifier& uuid = {});

  ~PersistableKeyValueStoreService() override;

  virtual bool persist() = 0;

 protected:
  bool setImpl(const utils::Identifier& key, const std::string& serialized_state) override;
  bool getImpl(const utils::Identifier& key, std::string& serialized_state) override;
  bool getImpl(std::map<utils::Identifier, std::string>& kvs) override;
  bool removeImpl(const utils::Identifier& key) override;
  bool persistImpl() override;
};

}  // namespace org::apache::nifi::minifi::controllers

#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_PERSISTABLEKEYVALUESTORESERVICE_H_
