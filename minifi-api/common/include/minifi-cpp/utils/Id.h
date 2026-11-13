// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <array>

#include "SmallString.h"

namespace org::apache::nifi::minifi::utils {

class Identifier {
  friend struct IdentifierTestAccessor;
  static constexpr const char* hex_lut = "0123456789abcdef";

 public:
  using Data = std::array<uint8_t, 16>;

  // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx is 36 long: 16 bytes * 2 hex digits / byte + 4 hyphens
  // only the string contents, excluding a null terminator
  static constexpr auto UUID_STR_LEN = 36;

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
  [[nodiscard]] SmallString<UUID_STR_LEN> to_string() const;

  static std::optional<Identifier> parse(const std::string& str);

 private:
  friend struct ::std::hash<org::apache::nifi::minifi::utils::Identifier>;

  static bool parseByte(Data& data, const uint8_t* input, int& charIdx, int& byteIdx);

  Data data_{};
};

}  // namespace org::apache::nifi::minifi::utils
