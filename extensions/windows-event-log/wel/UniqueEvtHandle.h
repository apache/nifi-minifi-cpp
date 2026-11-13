// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <Windows.h>

#include <memory>

namespace org::apache::nifi::minifi::wel {

struct unique_evt_handle_deleter {
  void operator()(EVT_HANDLE event) const noexcept { EvtClose(event); }
};

using unique_evt_handle = std::unique_ptr<std::remove_pointer<EVT_HANDLE>::type, unique_evt_handle_deleter>;

}  // namespace org::apache::nifi::minifi::wel
