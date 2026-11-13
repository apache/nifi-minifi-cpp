// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>

#include "sol/sol.hpp"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaOutputStream {
 public:
  explicit LuaOutputStream(std::shared_ptr<io::OutputStream> stream);

  /**
   * Write data (receives string, to follow Lua idioms)
   * @param buf
   * @return
   */
  size_t write(std::string buf);

 private:
  std::shared_ptr<io::OutputStream> stream_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
