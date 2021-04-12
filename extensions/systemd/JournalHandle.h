/**
 *
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
#include <thread>
#include <type_traits>

#include "utils/gsl.h"
#include "libwrapper/LibWrapper.h"

namespace org { namespace apache { namespace nifi { namespace minifi { namespace extensions { namespace systemd {
/**
 * A handle to the log journal for reading. Has to be used from the same thread throughout its lifetime.
 * @see man 8 sd_journal_open
 */
class JournalHandle final {
 public:
  explicit JournalHandle(std::unique_ptr<libwrapper::Journal>&&);
  JournalHandle(const JournalHandle&) = delete;
  JournalHandle(JournalHandle&&) = default;
  JournalHandle& operator=(const JournalHandle&) = delete;
  JournalHandle& operator=(JournalHandle&&) = default;
  ~JournalHandle();

  template<typename F>
  auto visit(F f) const -> decltype(f(std::declval<libwrapper::Journal&>())) {
    gsl_Expects(std::this_thread::get_id() == owner_thread_id_);
    return f(*handle_);
  }
 private:
  std::thread::id owner_thread_id_;
  std::unique_ptr<libwrapper::Journal> handle_;
};

}}}}}}  // namespace org::apache::nifi::minifi::extensions::systemd
