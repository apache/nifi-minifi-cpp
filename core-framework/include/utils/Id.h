// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <array>

#ifndef WIN32
class uuid;
#endif

#include "minifi-cpp/utils/Id.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/properties/Properties.h"
#include "utils/SmallString.h"
#include "utils/IdHash.h"

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

  std::array<unsigned char, 8> deterministic_prefix_{};
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
