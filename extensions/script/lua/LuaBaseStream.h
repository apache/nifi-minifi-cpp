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

#include <memory>
#include <string>

#include "sol/sol.hpp"
#include "io/BaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace lua {

class LuaBaseStream {
 public:
  explicit LuaBaseStream(std::shared_ptr<io::BaseStream> stream);

  /**
   * Read n bytes of data (returns string, to follow Lua idioms)
   * @return
   */
  std::string read(size_t len = 0);

  /**
   * Write data (receives string, to follow Lua idioms)
   * @param buf
   * @return
   */
  size_t write(std::string buf);

 private:
  std::shared_ptr<io::BaseStream> stream_;
};

} /* namespace lua */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
