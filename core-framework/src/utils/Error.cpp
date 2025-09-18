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

#include "utils/Error.h"

#include <cerrno>
#include "minifi-cpp/utils/gsl.h"

#ifdef WIN32
#include <windows.h>
#endif

namespace org::apache::nifi::minifi::utils {

std::error_code getLastError() {
#ifdef WIN32
  return {gsl::narrow<int>(GetLastError()), std::system_category()};
#else
  return {gsl::narrow<int>(errno), std::generic_category()};
#endif
}

bool compareErrors(const std::error_code lhs, const std::error_code rhs) {
  if (lhs.value() != rhs.value()) { return false; }
  if (lhs.category() == rhs.category()) { return true; }
  // Due to different static categories on windows across dll-s
  return std::string_view(lhs.category().name()) == std::string_view(rhs.category().name());
}


}  // namespace org::apache::nifi::minifi::utils
