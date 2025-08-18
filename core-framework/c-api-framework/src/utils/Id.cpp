/**
 *
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

#include "utils/Id.h"

#include "utils/StringUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

Identifier::Identifier(const Data& data) : data_(data) {}

Identifier& Identifier::operator=(const Data& data) {
  data_ = data;
  return *this;
}

Identifier& Identifier::operator=(const std::string& idStr) {
  const auto id = Identifier::parse(idStr);
  if (!id) {
    throw std::runtime_error("Couldn't parse UUID");
  }
  *this = id.value();
  return *this;
}

bool Identifier::isNil() const {
  return *this == Identifier{};
}

bool Identifier::operator!=(const Identifier& other) const {
  return !(*this == other);
}

bool Identifier::operator==(const Identifier& other) const {
  return data_ == other.data_;
}

bool Identifier::operator<(const Identifier &other) const {
  return data_ < other.data_;
}

SmallString<36> Identifier::to_string() const {
  SmallString<36> uuidStr;
  // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx is 36 long: 16 bytes * 2 hex digits / byte + 4 hyphens
  int byteIdx = 0;
  int charIdx = 0;

  // [xxxxxxxx]-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  while (byteIdx < 4) {
    uuidStr[charIdx++] = hex_lut[data_[byteIdx] >> 4];
    uuidStr[charIdx++] = hex_lut[data_[byteIdx++] & 0xf];
  }
  // xxxxxxxx[-]xxxx-xxxx-xxxx-xxxxxxxxxxxx
  uuidStr[charIdx++] = '-';

  // xxxxxxxx-[xxxx-xxxx-xxxx-]xxxxxxxxxxxx - 3x 2 bytes and a hyphen
  for (int idx = 0; idx < 3; ++idx) {
    uuidStr[charIdx++] = hex_lut[data_[byteIdx] >> 4];
    uuidStr[charIdx++] = hex_lut[data_[byteIdx++] & 0xf];
    uuidStr[charIdx++] = hex_lut[data_[byteIdx] >> 4];
    uuidStr[charIdx++] = hex_lut[data_[byteIdx++] & 0xf];
    uuidStr[charIdx++] = '-';
  }

  // xxxxxxxx-xxxx-xxxx-xxxx-[xxxxxxxxxxxx] - the rest, i.e. until byte 16
  while (byteIdx < 16) {
    uuidStr[charIdx++] = hex_lut[data_[byteIdx] >> 4];
    uuidStr[charIdx++] = hex_lut[data_[byteIdx++] & 0xf];
  }

  // null terminator
  uuidStr[charIdx] = 0;
  return uuidStr;
}

std::optional<Identifier> Identifier::parse(const std::string &str) {
  Identifier id;
  // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx is 36 long: 16 bytes * 2 hex digits / byte + 4 hyphens
  if (str.length() != 36) return {};
  int charIdx = 0;
  int byteIdx = 0;
  auto input = reinterpret_cast<const uint8_t*>(str.c_str());

  // [xxxxxxxx]-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  while (byteIdx < 4) {
    if (!parseByte(id.data_, input, charIdx, byteIdx)) return {};
  }
  // xxxxxxxx[-]xxxx-xxxx-xxxx-xxxxxxxxxxxx
  if (input[charIdx++] != '-') return {};

  // xxxxxxxx-[xxxx-xxxx-xxxx-]xxxxxxxxxxxx - 3x 2 bytes and a hyphen
  for (size_t idx = 0; idx < 3; ++idx) {
    if (!parseByte(id.data_, input, charIdx, byteIdx)) return {};
    if (!parseByte(id.data_, input, charIdx, byteIdx)) return {};
    if (input[charIdx++] != '-') return {};
  }

  // xxxxxxxx-xxxx-xxxx-xxxx-[xxxxxxxxxxxx] - the rest, i.e. until byte 16
  while (byteIdx < 16) {
    if (!parseByte(id.data_, input, charIdx, byteIdx)) return {};
  }
  return id;
}

bool Identifier::parseByte(Data &data, const uint8_t *input, int &charIdx, int &byteIdx) {
  uint8_t upper = 0;
  uint8_t lower = 0;
  if (!string::from_hex(input[charIdx++], upper)
      || !string::from_hex(input[charIdx++], lower)) {
    return false;
  }
  data[byteIdx++] = (upper << 4) | lower;
  return true;
}

}  // namespace org::apache::nifi::minifi::utils
