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

#include "core/extension/Extension.h"
#include "minifi-api.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessSession.h"
#include "minifi-cpp/core/controller/ControllerServiceContext.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi::utils {

inline minifi_string_view minifiStringView(const std::string_view s) {
  return minifi_string_view{.data = s.data(), .length = s.size()};
}

inline std::string toString(minifi_string_view sv) {
  return {sv.data, sv.length};
}

inline std::string_view toStringView(minifi_string_view sv) {
  return {sv.data, sv.length};
}

// Bidirectional type map between the C API's opaque handle types and their
// underlying C++ implementation types. A missing specialization at a call site
// is intentionally a compile error - any new opaque handle must be registered
// here before it can be passed through the generic cast helpers below.
template<typename C>
struct CppFor;
template<typename Cpp>
struct CFor;

#define MINIFI_API_MAP(CType, CppType) \
  template<>                           \
  struct CppFor<CType> {               \
    using type = CppType;              \
  };                                   \
  template<>                           \
  struct CFor<CppType> {               \
    using type = CType;                \
  }

MINIFI_API_MAP(minifi_process_context, minifi::core::ProcessContext);
MINIFI_API_MAP(minifi_process_session, minifi::core::ProcessSession);
MINIFI_API_MAP(minifi_controller_service_context, minifi::core::controller::ControllerServiceContext);
MINIFI_API_MAP(minifi_input_stream, minifi::io::InputStream);
MINIFI_API_MAP(minifi_output_stream, minifi::io::OutputStream);
MINIFI_API_MAP(minifi_extension, minifi::core::extension::Extension);
MINIFI_API_MAP(minifi_extension_context, minifi::core::extension::Extension::Context);
MINIFI_API_MAP(minifi_logger, std::shared_ptr<minifi::core::logging::Logger>);
MINIFI_API_MAP(minifi_flow_file, std::shared_ptr<minifi::core::FlowFile>);

#undef MINIFI_API_MAP

// Generic direct-pointer casts. Only usable for types registered above -
// otherwise the CppFor/CFor lookup fails to compile.
template<typename C>
auto* toCpp(C* c_ptr) noexcept {
  return reinterpret_cast<typename CppFor<C>::type*>(c_ptr);
}

template<typename Cpp>
auto* toC(Cpp* cpp_ptr) noexcept {
  return reinterpret_cast<typename CFor<Cpp>::type*>(cpp_ptr);
}

inline minifi::core::FlowFile* toRawFlowFile(minifi_flow_file* flow_file) {
  if (auto cpp_flow_file = toCpp(flow_file)) {
    return cpp_flow_file->get();
  }
  return nullptr;
}

}  // namespace org::apache::nifi::minifi::utils
