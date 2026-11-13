// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
