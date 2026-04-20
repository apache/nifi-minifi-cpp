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

#include <cstring>
#include <memory>
#include "utils/AWSSdkLogger.h"
#include "aws/core/Aws.h"
#include "minifi-c/minifi-c.h"
#include "utils/ExtensionInitUtils.h"
#include "minifi-cpp/agent/agent_version.h"

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)  // NOLINT(cppcoreguidelines-macro-usage)

namespace minifi = org::apache::nifi::minifi;

namespace org::apache::nifi::minifi::aws::init {
namespace {
template<size_t N>
MinifiStringView toStringView(const char (&s)[N]) {  // NOLINT(cppcoreguidelines-avoid-c-arrays): using template argument deduction for string literal length requires reference to array
  return MinifiStringView{.data = s, .length = N};
}
MinifiStringView toStringView(const char* s) {
  if (!s) {
    return MinifiStringView{.data = nullptr, .length = 0};
  }
  return MinifiStringView{.data = s, .length = std::strlen(s)};
}
}  // namespace

void deinit(gsl::owner<void*> sdk_opts_ptr) {
  std::unique_ptr<Aws::SDKOptions> sdk_options{static_cast<Aws::SDKOptions*>(sdk_opts_ptr)};

  Aws::Utils::Logging::ShutdownAWSLogging();
  Aws::ShutdownAPI(*sdk_options);
}
}  // namespace org::apache::nifi::minifi::aws::init

extern "C" void MinifiInitCppExtension(MinifiExtensionContext* extension_context) {
  using minifi::aws::init::toStringView;
  auto sdk_options = std::make_unique<Aws::SDKOptions>();
  Aws::InitAPI(*sdk_options);
  Aws::Utils::Logging::InitializeAWSLogging(std::make_shared<minifi::aws::utils::AWSSdkLogger>());

  MinifiExtensionDefinition ext_definition{.name = toStringView(MAKESTRING(MODULE_NAME)),
      .version = toStringView(minifi::AgentBuild::VERSION),
      .deinit = &minifi::aws::init::deinit,
      .user_data = sdk_options.get()};

  minifi::utils::MinifiRegisterCppExtension(extension_context, &ext_definition);
  std::ignore = sdk_options.release();  // ownership is transferred to deinit
}
