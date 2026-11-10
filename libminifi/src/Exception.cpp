// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "minifi-cpp/Exception.h"

#ifndef WIN32
#include <cxxabi.h>
#endif  // !WIN32

#include <typeinfo>

namespace org::apache::nifi::minifi {
std::string getCurrentExceptionTypeName() {
#ifndef WIN32
  const std::type_info* exception_type = abi::__cxa_current_exception_type();
  if (exception_type) {
    return exception_type->name();
  }
#endif  // !WIN32
  try {
    std::rethrow_exception(std::current_exception());
  } catch (const std::exception& ex) {
    return typeid(ex).name();
  } catch (...) { }

  return {};
}
}  // namespace org::apache::nifi::minifi
