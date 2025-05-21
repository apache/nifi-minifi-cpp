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

#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <bit>

namespace org::apache::nifi::minifi::modbus {

template<typename T, std::endian to_endianness = std::endian::big>
constexpr std::array<std::byte, std::max(sizeof(T), sizeof(uint16_t))> toBytes(T value) {
  std::array<std::byte, std::max(sizeof(T), sizeof(uint16_t))> buffer{};

  std::copy_n(reinterpret_cast<std::byte*>(&value), sizeof(T), buffer.begin());

  if constexpr (std::endian::native != to_endianness) {
    std::reverse(buffer.begin(), buffer.end());
  }

  return buffer;
}

template<typename T, std::endian from_endianness = std::endian::big>
constexpr T fromBytes(std::array<std::byte, std::max(sizeof(T), sizeof(uint16_t))> bytes) {
  if constexpr (std::endian::native != from_endianness) {
    std::reverse(bytes.begin(), bytes.end());
  }

  T result;
  std::memcpy(&result, bytes.data(), sizeof(T));
  return result;
}
}  // namespace org::apache::nifi::minifi::modbus
