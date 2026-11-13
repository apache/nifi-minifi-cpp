// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "minifi-cpp/core/ClassLoader.h"
#include "utils/GeneralUtils.h"

namespace org::apache::nifi::minifi::core {

template<class T>
std::unique_ptr<T> ClassLoader::instantiate(const std::string &class_name, const std::string &name) {
  return utils::dynamic_unique_cast<T>(instantiate(class_name, name, [] (CoreComponent* obj) -> bool {
    return dynamic_cast<T*>(obj) != nullptr;
  }));
}

template<class T>
std::unique_ptr<T> ClassLoader::instantiate(const std::string &class_name, const utils::Identifier &uuid) {
  return utils::dynamic_unique_cast<T>(instantiate(class_name, uuid, [] (CoreComponent* obj) -> bool {
    return dynamic_cast<T*>(obj) != nullptr;
  }));
}

template<class T>
gsl::owner<T*> ClassLoader::instantiateRaw(const std::string &class_name, const std::string &name) {
  return dynamic_cast<T*>(instantiateRaw(class_name, name, [] (CoreComponent* obj) -> bool {
    return dynamic_cast<T*>(obj) != nullptr;
  }));
}

template<class T>
std::unique_ptr<T> ClassLoader::instantiate(const std::string &class_name, const std::string &name, const utils::Identifier &uuid) {
  return utils::dynamic_unique_cast<T>(instantiate(class_name, name, uuid, [] (CoreComponent* obj) -> bool {
    return dynamic_cast<T*>(obj) != nullptr;
  }));
}

}  // namespace org::apache::nifi::minifi::core
