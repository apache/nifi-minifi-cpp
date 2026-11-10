// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/TimeUtil.h"
#include "minifi-cpp/utils/TimeUtil.h"

#ifdef WIN32
#include "date/tz.h"
#endif

namespace org::apache::nifi::minifi::utils::timeutils {

static std::mutex global_clock_mtx;
static std::shared_ptr<SteadyClock> global_clock{std::make_shared<SteadyClock>()};

std::shared_ptr<SteadyClock> getClock() {
  std::lock_guard lock(global_clock_mtx);
  return global_clock;
}

// test-only utility to specify what clock to use
void setClock(std::shared_ptr<SteadyClock> clock) {
  std::lock_guard lock(global_clock_mtx);
  global_clock = std::move(clock);
}

#ifdef WIN32
void dateSetGlobalInstall(const std::string& install) {
  date::set_install(install);
}
#endif


}  // namespace org::apache::nifi::minifi::utils::timeutils

