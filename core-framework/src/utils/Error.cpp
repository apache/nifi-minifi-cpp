// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
