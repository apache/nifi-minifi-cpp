// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>

namespace org::apache::nifi::minifi::utils::timeutils {

#ifdef WIN32
// The tzdata location is set as a global variable in date-tz library
// We need to set it from from libminifi to effect calls made from libminifi (on Windows)
void dateSetInstall(const std::string& install);
#endif

}  // namespace org::apache::nifi::minifi::utils::timeutils
