// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace org::apache::nifi::minifi::utils::trace_resolver {

struct BackTrace {
  std::string name;
  std::vector<std::string> trace;
};

BackTrace getBackTrace(std::string_view thread_name, std::thread::native_handle_type thread);
BackTrace getBackTrace(std::string_view thread_name);

}  // namespace org::apache::nifi::minifi::utils::trace_resolver
