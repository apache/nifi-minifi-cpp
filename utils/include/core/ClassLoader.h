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

#include "minifi-cpp/core/ClassLoader.h"
#include "utils/GeneralUtils.h"
#include "ObjectFactory.h"

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
T *ClassLoader::instantiateRaw(const std::string &class_name, const std::string &name) {
  return dynamic_cast<T*>(instantiateRaw(class_name, name, [] (CoreComponent* obj) -> bool {
    return dynamic_cast<T*>(obj) != nullptr;
  }));
}

}  // namespace org::apache::nifi::minifi::core
