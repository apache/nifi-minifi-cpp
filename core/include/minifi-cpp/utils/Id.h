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

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <array>

#include "SmallString.h"

namespace org::apache::nifi::minifi::utils {

class Identifier {
  friend struct IdentifierTestAccessor;
  static constexpr const char* hex_lut = "0123456789abcdef";

 public:
  using Data = std::array<uint8_t, 16>;

  Identifier() = default;
  explicit Identifier(const Data& data);
  Identifier &operator=(const Data& data);

  Identifier &operator=(const std::string& idStr);

  explicit operator bool() const {
    return !isNil();
  }

  bool operator!=(const Identifier& other) const;
  bool operator==(const Identifier& other) const;
  bool operator<(const Identifier& other) const;

  bool isNil() const;

  // Numerous places query the string representation
  // just to then forward the temporary to build logs,
  // streams, or others. Dynamically allocating in these
  // instances is wasteful as we immediately discard
  // the result. The difference on the test machine is 8x,
  // building the representation itself takes 10ns, while
  // subsequently turning it into a std::string would take
  // 70ns more.
  SmallString<36> to_string() const;

  static std::optional<Identifier> parse(const std::string& str);

 private:
  friend struct ::std::hash<org::apache::nifi::minifi::utils::Identifier>;

  static bool parseByte(Data& data, const uint8_t* input, int& charIdx, int& byteIdx);

  Data data_{};
};

}  // namespace org::apache::nifi::minifi::utils
