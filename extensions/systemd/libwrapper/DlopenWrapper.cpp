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
#include "DlopenWrapper.h"

#include <dlfcn.h>

#include "minifi-cpp/Exception.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/StringUtils.h"

struct sd_journal;

namespace org::apache::nifi::minifi::extensions::systemd::libwrapper {

namespace {
struct dlclose_deleter {
  void operator()(void* const libhandle) { if (libhandle) dlclose(libhandle); }
};
}  // namespace

class DlopenJournal : public Journal {
 public:
  explicit DlopenJournal(const JournalType type) {
    constexpr auto SD_JOURNAL_LOCAL_ONLY = 1 << 0;
    constexpr auto SD_JOURNAL_SYSTEM = 1 << 2;
    constexpr auto SD_JOURNAL_CURRENT_USER = 1 << 3;
    const int flags{(type == JournalType::User ? SD_JOURNAL_CURRENT_USER : 0)
        | (type == JournalType::System ? SD_JOURNAL_SYSTEM : 0)
        | SD_JOURNAL_LOCAL_ONLY};
    const int error_code = open_(&j_, flags);
    if (error_code < 0) {
      // sd_journal_open returns negative errno values
      const auto err_cond = std::generic_category().default_error_condition(-error_code);
      throw SystemErrorException{"sd_journal_open", err_cond};
    }
  }
  DlopenJournal(const DlopenJournal&) = delete;
  DlopenJournal(DlopenJournal&&) = delete;
  DlopenJournal& operator=(const DlopenJournal&) = delete;
  DlopenJournal& operator=(DlopenJournal&&) = delete;

  ~DlopenJournal() override {
    if (j_ && close_) close_(j_);
  }

  int seekHead() noexcept override { return seek_head_(j_); }
  int seekTail() noexcept override { return seek_tail_(j_); }
  int seekCursor(const char* const cursor) noexcept override { return seek_cursor_(j_, cursor); }

  int getCursor(gsl::owner<char*>* const cursor_out) noexcept override { return get_cursor_(j_, cursor_out); }

  int next() noexcept override { return next_(j_); }
  int enumerateData(const void** const data_out, size_t* const size_out) noexcept override { return enumerate_data_(j_, data_out, size_out); }

  int getRealtimeUsec(uint64_t* const usec_out) noexcept override { return get_realtime_usec_(j_, usec_out); }

 private:
  template<typename F>
  F loadSymbol(const char* const symbol_name) {
    // The cast below is supported by POSIX platforms. https://stackoverflow.com/a/1096349
    F const symbol = (F)dlsym(libhandle_.get(), symbol_name);
    const char* const err = dlerror();
    if (err) throw Exception(ExceptionType::GENERAL_EXCEPTION, utils::string::join_pack("dlsym(", symbol_name, "): ", err));
    return symbol;
  }

  std::unique_ptr<void, dlclose_deleter> libhandle_{[] {
    auto* const handle = dlopen("libsystemd.so.0", RTLD_LAZY);
    if (!handle) throw Exception(ExceptionType::GENERAL_EXCEPTION, utils::string::join_pack("dlopen failed: ", dlerror()));
    return handle;
  }()};

  int (*open_)(sd_journal**, int) = loadSymbol<decltype(open_)>("sd_journal_open");
  void (*close_)(sd_journal*) = loadSymbol<decltype(close_)>("sd_journal_close");
  int (*seek_head_)(sd_journal*) = loadSymbol<decltype(seek_head_)>("sd_journal_seek_head");
  int (*seek_tail_)(sd_journal*) = loadSymbol<decltype(seek_tail_)>("sd_journal_seek_tail");
  int (*seek_cursor_)(sd_journal*, const char*) = loadSymbol<decltype(seek_cursor_)>("sd_journal_seek_cursor");
  int (*get_cursor_)(sd_journal*, char**) = loadSymbol<decltype(get_cursor_)>("sd_journal_get_cursor");
  int (*next_)(sd_journal*) = loadSymbol<decltype(next_)>("sd_journal_next");
  int (*enumerate_data_)(sd_journal*, const void**, size_t*) = loadSymbol<decltype(enumerate_data_)>("sd_journal_enumerate_data");
  int (*get_realtime_usec_)(sd_journal*, uint64_t*) = loadSymbol<decltype(get_realtime_usec_)>("sd_journal_get_realtime_usec");

  gsl::owner<sd_journal*> j_ = nullptr;
};

std::unique_ptr<Journal> DlopenWrapper::openJournal(const JournalType type) {
  return std::make_unique<DlopenJournal>(type);
}

}  // namespace org::apache::nifi::minifi::extensions::systemd::libwrapper
