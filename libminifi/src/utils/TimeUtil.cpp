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


#include "utils/TimeUtil.h"
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

