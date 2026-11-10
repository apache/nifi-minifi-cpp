// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

// The Windows version of pyconfig.h (https://github.com/python/cpython/blob/3.10/PC/pyconfig.h, also on main as of 2022-08-18)
// rather unhelpfully #define's COMPILER as the detected compiler version.  Since we have a COMPILER variable below, we need to #undef it
// to make anything which requires cpython (e.g. the script extension) compile on Windows.
#undef COMPILER

#include <string>
#include <vector>
#include "minifi-cpp/utils/Export.h"

namespace org::apache::nifi::minifi {

class AgentBuild {
 public:
  MINIFIAPI static const char* const VERSION;
  MINIFIAPI static const char* const BUILD_IDENTIFIER;
  MINIFIAPI static const char* const BUILD_REV;
  MINIFIAPI static const char* const BUILD_DATE;
  MINIFIAPI static const char* const COMPILER;
  MINIFIAPI static const char* const COMPILER_VERSION;
  MINIFIAPI static const char* const COMPILER_FLAGS;
};

}  // namespace namespace org::apache::nifi::minifi
