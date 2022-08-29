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

#ifndef WIN32
class uuid;
#endif

#include "core/logging/Logger.h"
#include "properties/Properties.h"
#include "SmallString.h"
#include "Hash.h"

#define UUID_TIME_IMPL 0
#define UUID_RANDOM_IMPL 1
#define UUID_DEFAULT_IMPL 2
#define MINIFI_UID_IMPL 3

#define UUID_RANDOM_STR "random"
#define UUID_WINDOWS_RANDOM_STR "windows_random"
#define UUID_DEFAULT_STR "uuid_default"
#define MINIFI_UID_STR "minifi_uid"
#define UUID_TIME_STR "time"
#define UUID_WINDOWS_STR "windows"

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

class IdGenerator {
 public:
  Identifier generate();
  void initialize(const std::shared_ptr<Properties>& properties);

  ~IdGenerator();

  static std::shared_ptr<IdGenerator> getIdGenerator() {
    static std::shared_ptr<IdGenerator> generator = std::shared_ptr<IdGenerator>(new IdGenerator());
    return generator;
  }

 protected:
  uint64_t getDeviceSegmentFromString(const std::string& str, int numBits) const;
  uint64_t getRandomDeviceSegment(int numBits) const;

 private:
  IdGenerator();
  int implementation_;
  std::shared_ptr<minifi::core::logging::Logger> logger_;

  unsigned char deterministic_prefix_[8];
  std::atomic<uint64_t> incrementor_;

  std::mutex uuid_mutex_;
#ifndef WIN32
  std::unique_ptr<uuid> uuid_impl_;
  bool generateWithUuidImpl(unsigned int mode, Identifier::Data& output);
#endif
};

class NonRepeatingStringGenerator {
 public:
  std::string generate() {
    return prefix_ + std::to_string(incrementor_++);
  }
  uint64_t generateId() {
    return incrementor_++;
  }
 private:
  std::atomic<uint64_t> incrementor_{0};
  std::string prefix_{IdGenerator::getIdGenerator()->generate().to_string() + "-"};
};

}  // namespace org::apache::nifi::minifi::utils

namespace std {
template<>
struct hash<org::apache::nifi::minifi::utils::Identifier> {
  size_t operator()(const org::apache::nifi::minifi::utils::Identifier& id) const noexcept {
    static_assert(sizeof(org::apache::nifi::minifi::utils::Identifier) % sizeof(size_t) == 0);
    constexpr int slices = sizeof(org::apache::nifi::minifi::utils::Identifier) / sizeof(size_t);
    const auto get_slice = [](const org::apache::nifi::minifi::utils::Identifier& id, size_t idx) -> size_t {
      size_t result{};
      memcpy(&result, reinterpret_cast<const unsigned char*>(&id.data_) + idx * sizeof(size_t), sizeof(size_t));
      return result;
    };
    size_t hash = get_slice(id, 0);
    for (size_t i = 1; i < slices; ++i) {
      hash = org::apache::nifi::minifi::utils::hash_combine(hash, get_slice(id, i));
    }
    return hash;
  }
};
}  // namespace std
