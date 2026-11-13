// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>

#include "sol/sol.hpp"
#include "minifi-cpp/io/InputStream.h"

namespace org::apache::nifi::minifi::extensions::lua {

class LuaInputStream {
 public:
  explicit LuaInputStream(std::shared_ptr<io::InputStream> stream);

  /**
   * Read n bytes of data (returns string, to follow Lua idioms)
   * @return
   */
  std::string read(size_t len = 0);

 private:
  std::shared_ptr<io::InputStream> stream_;
};

}  // namespace org::apache::nifi::minifi::extensions::lua
