// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/TimeZoneUtils.h"

#include "minifi-cpp/utils/TimeUtil.h"

#ifdef WIN32
#include "date/tz.h"
#endif

namespace org::apache::nifi::minifi::utils::timeutils {

#ifdef WIN32
// If minifi is not installed through the MSI installer, then TZDATA might be missing
// date::set_install can point to the TZDATA location, but it has to be called from each library/executable that wants to use timezones
void dateSetInstall(const std::string& install) {
  date::set_install(install);
  dateSetGlobalInstall(install);
}
#endif

}  // namespace org::apache::nifi::minifi::utils::timeutils
