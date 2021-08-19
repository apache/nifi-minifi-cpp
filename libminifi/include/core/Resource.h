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
#ifndef LIBMINIFI_INCLUDE_CORE_RESOURCE_H_
#define LIBMINIFI_INCLUDE_CORE_RESOURCE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include "ClassLoader.h"
#include "agent/agent_docs.h"
#include "utils/OptionalUtils.h"
#include "utils/Macro.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)

static inline ClassLoader& getClassLoader() {
#ifdef MODULE_NAME
  return ClassLoader::getDefaultClassLoader().getClassLoader(MAKESTRING(MODULE_NAME));
#else
  return ClassLoader::getDefaultClassLoader();
#endif
}

template<class T>
class StaticClassType {
 public:
  StaticClassType(const std::string& name, const std::optional<std::string>& description, const std::vector<std::string>& construction_names)
      : name_(name), construction_names_(construction_names) {
    // Notify when the static member is created
    if (description) {
      minifi::AgentDocs::putDescription(name, description.value());
    }
    for (const auto& construction_name : construction_names_) {
#ifdef MODULE_NAME
      auto factory = std::unique_ptr<ObjectFactory>(new DefautObjectFactory<T>(MAKESTRING(MODULE_NAME)));
#else
      auto factory = std::unique_ptr<ObjectFactory>(new DefautObjectFactory<T>("minifi-system"));
#endif
      getClassLoader().registerClass(construction_name, std::move(factory));
    }
  }

  ~StaticClassType() {
    for (const auto& construction_name : construction_names_) {
      getClassLoader().unregisterClass(construction_name);
    }
  }

  static StaticClassType& get(const std::string& name, const std::optional<std::string> &description, const std::vector<std::string>& construction_names) {
    static StaticClassType instance(name, description, construction_names);
    return instance;
  }

 private:
  std::string name_;
  std::vector<std::string> construction_names_;
};

#define MAKE_INIT_LIST(...) {FOR_EACH(IDENTITY, COMMA, (__VA_ARGS__))}


#define REGISTER_RESOURCE(CLASSNAME, DESC) \
        static auto& CLASSNAME##_registrar = core::StaticClassType<CLASSNAME>::get(#CLASSNAME, DESC, {#CLASSNAME})

#define REGISTER_RESOURCE_AS(CLASSNAME, DESC, NAMES) \
        static auto& CLASSNAME##_registrar = core::StaticClassType<CLASSNAME>::get(#CLASSNAME, DESC, MAKE_INIT_LIST NAMES)

#define REGISTER_INTERNAL_RESOURCE(CLASSNAME) \
        static auto& CLASSNAME##_registrar = core::StaticClassType<CLASSNAME>::get(#CLASSNAME, {}, {#CLASSNAME})

#define REGISTER_INTERNAL_RESOURCE_AS(CLASSNAME, NAMES) \
        static auto& CLASSNAME##_registrar = core::StaticClassType<CLASSNAME>::get(#CLASSNAME, {}, MAKE_INIT_LIST NAMES)


}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_RESOURCE_H_
