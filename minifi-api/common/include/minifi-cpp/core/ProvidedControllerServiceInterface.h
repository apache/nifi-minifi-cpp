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

#include <string_view>
#include <type_traits>

#include "core/ClassName.h"

namespace org::apache::nifi::minifi::core {

struct ProvidedInterface {
  /// fully-qualified class name of the implemented ControllerService interface
  std::string_view name;
  /// casts a pointer of the ControllerService from its dynamic type to a pointer of an implemented interface type. Needed to support cases where they
  /// are not the same, like multiple inheritance, virtual inheritance, or contained non-first member implementing the interface
  void* (*cast)(void* instance);
};

/// casts a pointer of the ControllerService from its dynamic type to a pointer of an implemented interface type. Needed to support cases where they
/// are not the same, like multiple inheritance, virtual inheritance, or contained non-first member implementing the interface
template<typename Interface, typename ConcreteService>
void* castProvidedControllerServiceToInterface(void* instance) {
  auto* concrete = static_cast<ConcreteService*>(instance);
  auto* interface_ptr = static_cast<Interface*>(concrete);
  return interface_ptr;
}

template<typename Interface, typename ConcreteService>
constexpr ProvidedInterface createProvidedInterface() {
  return ProvidedInterface{className<Interface>(), &castProvidedControllerServiceToInterface<Interface, ConcreteService>};
}

}  // namespace org::apache::nifi::minifi::core
