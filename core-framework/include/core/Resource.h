// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
#include "core/ProcessorFactoryImpl.h"
#include "core/controller/ControllerServiceFactoryImpl.h"
#include "core/ObjectFactory.h"
#include "minifi-cpp/agent/agent_version.h"

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
#ifdef MINIFI_EXTENSION_GROUP_NAME
    auto group_name = MAKESTRING(MINIFI_EXTENSION_GROUP_NAME);
#else
    static_assert(false, "MINIFI_EXTENSION_GROUP_NAME has to be defined");
#endif

    if constexpr (Type == ResourceType::Processor) {
      for (const auto& construction_name : construction_names_) {
        auto factory = std::unique_ptr<ProcessorFactory>(new ProcessorFactoryImpl<Class>(module_name));
        getClassLoader().registerClass(construction_name, std::move(factory));
      }
    } else if constexpr (Type == ResourceType::ControllerService) {
      for (const auto& construction_name : construction_names_) {
        auto factory = std::unique_ptr<controller::ControllerServiceFactory>(new controller::ControllerServiceFactoryImpl<Class>(module_name));
        getClassLoader().registerClass(construction_name, std::move(factory));
      }
    } else {
      for (const auto& construction_name : construction_names_) {
        auto factory = std::unique_ptr<ObjectFactory>(new DefaultObjectFactory<Class>(module_name));
        getClassLoader().registerClass(construction_name, std::move(factory));
      }
    }

    minifi::BundleCoordinate bundle_coord{.name = module_name, .group_name = group_name, .version = minifi::AgentBuild::VERSION};
    minifi::ClassDescriptionRegistry::createClassDescription<Class, Type>(bundle_coord, class_name);
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

#define REGISTER_RESOURCE_IMPLEMENTATION(CLASS, CLASSNAME, TYPE) \
        static const auto& CLASS##_registrar = core::StaticClassType<CLASS, minifi::ResourceType::TYPE>::get(CLASSNAME, {CLASSNAME})

}  // namespace org::apache::nifi::minifi::core
