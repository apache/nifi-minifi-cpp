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
#pragma once

#include "Exception.h"
#include "LibWrapper.h"
#include <dlfcn.h>
#include <memory>

struct sd_journal;

namespace org::apache::nifi::minifi::extensions::systemd::libwrapper {

namespace detail {
struct dlclose_deleter {
  void operator()(void*);
};
}  // namespace detail

class DlopenJournal;

class DlopenWrapper : public LibWrapper {
 public:
  std::unique_ptr<Journal> openJournal(JournalType) override;
  int notify(const bool unset_environment, const char* const state) override { return (*notify_)(unset_environment, state); }

 private:
  friend class DlopenJournal;

  template<typename F>
  gsl::not_null<F> loadSymbol(const char* const symbol_name) {
    // The cast below is supported by POSIX platforms. https://stackoverflow.com/a/1096349
    F const symbol = (F)dlsym(libhandle_.get(), symbol_name);
    const char* const err = dlerror();
    if (!symbol || err) throw Exception(ExceptionType::GENERAL_EXCEPTION, utils::StringUtils::join_pack("dlsym(", symbol_name, "): ", err));
    return gsl::make_not_null(symbol);
  }

  std::unique_ptr<void, detail::dlclose_deleter> libhandle_{[] {
    auto* const handle = dlopen("libsystemd.so.0", RTLD_LAZY);
    if (!handle) throw Exception(ExceptionType::GENERAL_EXCEPTION, utils::StringUtils::join_pack("dlopen failed: ", dlerror()));
    return handle;
  }()};

  gsl::not_null<int (*)(sd_journal**, int)> open_ = loadSymbol<int(*)(sd_journal**, int)>("sd_journal_open");
  gsl::not_null<void (*)(sd_journal*)> close_ = loadSymbol<void (*)(sd_journal*)>("sd_journal_close");
  gsl::not_null<int (*)(sd_journal*)> seek_head_ = loadSymbol<int(*)(sd_journal*)>("sd_journal_seek_head");
  gsl::not_null<int (*)(sd_journal*)> seek_tail_ = loadSymbol<int(*)(sd_journal*)>("sd_journal_seek_tail");
  gsl::not_null<int (*)(sd_journal*, const char*)> seek_cursor_ = loadSymbol<int(*)(sd_journal*, const char*)>("sd_journal_seek_cursor");
  gsl::not_null<int (*)(sd_journal*, char**)> get_cursor_ = loadSymbol<int (*)(sd_journal*, char**)>("sd_journal_get_cursor");
  gsl::not_null<int (*)(sd_journal*)> next_ = loadSymbol<int (*)(sd_journal*)>("sd_journal_next");
  gsl::not_null<int (*)(sd_journal*, const void**, size_t*)> enumerate_data_ = loadSymbol<int (*)(sd_journal*, const void**, size_t*)>("sd_journal_enumerate_data");
  gsl::not_null<int (*)(sd_journal*, uint64_t*)> get_realtime_usec_ = loadSymbol<int (*)(sd_journal*, uint64_t*)>("sd_journal_get_realtime_usec");
  gsl::not_null<int (*)(int, const char*)> notify_ = loadSymbol<int (*)(int, const char*)>("sd_notify");
};

}  // namespace org::apache::nifi::minifi::extensions::systemd::libwrapper
