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

#include "../Common.h"
#include "utils/gsl.h"
#include "range/v3/algorithm/transform.hpp"

namespace org::apache::nifi::minifi::extensions::systemd::libwrapper {

struct Journal {
  Journal() = default;
  Journal(const Journal&) = delete;
  Journal(Journal&&) = delete;
  Journal& operator=(const Journal&) = delete;
  Journal& operator=(Journal&&) = delete;
  virtual ~Journal() = default;

  virtual int seekHead() noexcept = 0;
  virtual int seekTail() noexcept = 0;
  virtual int seekCursor(const char*) noexcept = 0;

  virtual int getCursor(gsl::owner<char*>* cursor_out) noexcept = 0;

  virtual int next() noexcept = 0;
  virtual int enumerateData(const void** data_out, size_t* size_out) noexcept = 0;

  virtual int getRealtimeUsec(uint64_t* usec_out) noexcept = 0;
};


struct LibWrapper {
  LibWrapper() = default;
  LibWrapper(const LibWrapper&) = delete;
  LibWrapper(LibWrapper&&) = delete;
  LibWrapper& operator=(const LibWrapper&) = delete;
  LibWrapper& operator=(LibWrapper&&) = delete;
  virtual ~LibWrapper() = default;

  virtual std::unique_ptr<Journal> openJournal(JournalType) = 0;
  virtual int notify(bool unset_environment, const char* state) = 0;
};

std::unique_ptr<LibWrapper> createLibWrapper();

}  // namespace org::apache::nifi::minifi::extensions::systemd::libwrapper
