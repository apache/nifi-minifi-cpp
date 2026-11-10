// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <cstdint>

#include "../Common.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::systemd::libwrapper {

struct Journal {
  virtual int seekHead() noexcept = 0;
  virtual int seekTail() noexcept = 0;
  virtual int seekCursor(const char*) noexcept = 0;

  virtual int getCursor(gsl::owner<char*>* cursor_out) noexcept = 0;

  virtual int next() noexcept = 0;
  virtual int enumerateData(const void** data_out, size_t* size_out) noexcept = 0;

  virtual int getRealtimeUsec(uint64_t* usec_out) noexcept = 0;

  virtual ~Journal() = default;
};


struct LibWrapper {
  virtual std::unique_ptr<Journal> openJournal(JournalType) = 0;
  virtual ~LibWrapper() = default;
};

std::unique_ptr<LibWrapper> createLibWrapper();

}  // namespace org::apache::nifi::minifi::extensions::systemd::libwrapper
