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

namespace org::apache::nifi::minifi::core {

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)

static inline ClassLoader& getClassLoader() {
#ifdef MODULE_NAME
  return ClassLoader::getDefaultClassLoader().getClassLoader(MAKESTRING(MODULE_NAME));
#else
  return ClassLoader::getDefaultClassLoader();
#endif
}

template<typename Class, ResourceType Type>
class StaticClassType {
 public:
  StaticClassType(const std::string& class_name, const std::vector<std::string>& construction_names)
      : name_(class_name), construction_names_(construction_names) {
#ifdef MODULE_NAME
      auto module_name = MAKESTRING(MODULE_NAME);
#else
      auto module_name = "minifi-system";
#endif

    for (const auto& construction_name : construction_names_) {
      auto factory = std::unique_ptr<ObjectFactory>(new DefaultObjectFactory<Class>(module_name));
      getClassLoader().registerClass(construction_name, std::move(factory));
    }

    minifi::AgentDocs::createClassDescription<Class, Type>(module_name, class_name);
  }

  ~StaticClassType() {
    for (const auto& construction_name : construction_names_) {
      getClassLoader().unregisterClass(construction_name);
    }
  }
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif
  static const StaticClassType& get(const std::string& name, const std::vector<std::string>& construction_names) {
    static const StaticClassType instance(name, construction_names);
    return instance;
  }
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

 private:
  std::string name_;
  std::vector<std::string> construction_names_;
};

#define MAKE_INIT_LIST(...) {FOR_EACH(IDENTITY, COMMA, (__VA_ARGS__))}

#define REGISTER_RESOURCE(CLASSNAME, TYPE) \
        static const auto& CLASSNAME##_registrar = core::StaticClassType<CLASSNAME, minifi::ResourceType::TYPE>::get(#CLASSNAME, {#CLASSNAME})

#define REGISTER_RESOURCE_AS(CLASSNAME, TYPE, NAMES) \
        static const auto& CLASSNAME##_registrar = core::StaticClassType<CLASSNAME, minifi::ResourceType::TYPE>::get(#CLASSNAME, MAKE_INIT_LIST NAMES)

}  // namespace org::apache::nifi::minifi::core
