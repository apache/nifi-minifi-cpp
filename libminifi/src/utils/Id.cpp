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

// This needs to be included first to let uuid.h sort out the system header collisions
#ifndef WIN32
#include "uuid++.hh"
#endif

#include "utils/Id.h"

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <limits>
#include "core/logging/LoggerConfiguration.h"

#ifdef WIN32
#include "Rpc.h"
#include "Winsock2.h"
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#ifdef WIN32
namespace {
  void windowsUuidToUuidField(UUID* uuid, Identifier::Data& out) {
    uint32_t Data1BE = htonl(uuid->Data1);
    memcpy(out.data(), &Data1BE, 4);
    uint16_t Data2BE = htons(uuid->Data2);
    memcpy(out.data() + 4, &Data2BE, 2);
    uint16_t Data3BE = htons(uuid->Data3);
    memcpy(out.data() + 6, &Data3BE, 2);
    memcpy(out.data() + 8, uuid->Data4, 8);
  }

  void windowsUuidGenerateTime(Identifier::Data& out) {
    UUID uuid;
    UuidCreateSequential(&uuid);
    windowsUuidToUuidField(&uuid, out);
  }

  void windowsUuidGenerateRandom(Identifier::Data& out) {
    UUID uuid;
    UuidCreate(&uuid);
    windowsUuidToUuidField(&uuid, out);
  }
}  // namespace
#endif

Identifier::Identifier(const Data& data) : data_(data) {}

Identifier& Identifier::operator=(const Data& data) {
  data_ = data;
  return *this;
}

Identifier& Identifier::operator=(const std::string& idStr) {
  utils::optional<Identifier> id = Identifier::parse(idStr);
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

bool Identifier::parseByte(Data &data, const uint8_t *input, int &charIdx, int &byteIdx) {
  uint8_t upper, lower;
  if (!StringUtils::from_hex(input[charIdx++], upper)
      || !StringUtils::from_hex(input[charIdx++], lower)) {
    return false;
  }
  data[byteIdx++] = (upper << 4) | lower;
  return true;
}

IdGenerator::IdGenerator()
    : implementation_(UUID_TIME_IMPL),
      logger_(logging::LoggerFactory<IdGenerator>::getLogger()),
      incrementor_(0) {
#ifndef WIN32
  uuid_impl_ = std::unique_ptr<uuid>(new uuid());
#endif
}

IdGenerator::~IdGenerator() = default;

uint64_t IdGenerator::getDeviceSegmentFromString(const std::string& str, int numBits) const {
  uint64_t deviceSegment = 0;
  for (size_t i = 0; i < str.length(); i++) {
    unsigned char c = toupper(str[i]);
    if (c >= '0' && c <= '9') {
      deviceSegment = deviceSegment + (c - '0');
    } else if (c >= 'A' && c <= 'F') {
      deviceSegment = deviceSegment + (c - 'A' + 10);
    } else {
      logging::LOG_ERROR(logger_) << "Expected hex char (0-9, A-F).  Got " << c;
    }
    deviceSegment = deviceSegment << 4;
  }
  deviceSegment <<= 64 - (4 * (str.length() + 1));
  deviceSegment >>= 64 - numBits;
  logging::LOG_DEBUG(logger_) << "Using user defined device segment: " << std::hex << deviceSegment;
  deviceSegment <<= 64 - numBits;
  return deviceSegment;
}

uint64_t IdGenerator::getRandomDeviceSegment(int numBits) const {
  uint64_t deviceSegment = 0;
  Identifier::Data random_uuid{};
  for (int word = 0; word < 2; word++) {
#ifdef WIN32
    windowsUuidGenerateRandom(random_uuid);
#else
    uuid temp_uuid;
    temp_uuid.make(UUID_MAKE_V4);
    void* uuid_bin = temp_uuid.binary();
    memcpy(random_uuid.data(), uuid_bin, 16);
    free(uuid_bin);
#endif
    for (int i = 0; i < 4; i++) {
      deviceSegment += random_uuid[i];
      deviceSegment <<= 8;
    }
  }
  deviceSegment >>= 64 - numBits;
  logging::LOG_DEBUG(logger_) << "Using random defined device segment:" << deviceSegment;
  deviceSegment <<= 64 - numBits;
  return deviceSegment;
}

void IdGenerator::initialize(const std::shared_ptr<Properties>& properties) {
  std::string implementation_str;
  implementation_ = UUID_TIME_IMPL;
  if (properties->getString("uid.implementation", implementation_str)) {
    std::transform(implementation_str.begin(), implementation_str.end(), implementation_str.begin(), ::tolower);
    if (UUID_RANDOM_STR == implementation_str || UUID_WINDOWS_RANDOM_STR == implementation_str) {
      logging::LOG_DEBUG(logger_) << "Using uuid_generate_random for uids.";
      implementation_ = UUID_RANDOM_IMPL;
    } else if (UUID_DEFAULT_STR == implementation_str) {
      logging::LOG_DEBUG(logger_) << "Using uuid_generate for uids.";
      implementation_ = UUID_DEFAULT_IMPL;
    } else if (MINIFI_UID_STR == implementation_str) {
      logging::LOG_DEBUG(logger_) << "Using minifi uid implementation for uids";
      implementation_ = MINIFI_UID_IMPL;

      uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      int device_bits = properties->getInt("uid.minifi.device.segment.bits", 16);
      std::string device_segment;
      uint64_t prefix = timestamp;
      if (device_bits > 0) {
        if (properties->getString("uid.minifi.device.segment", device_segment)) {
          prefix = getDeviceSegmentFromString(device_segment, device_bits);
        } else {
          logging::LOG_WARN(logger_) << "uid.minifi.device.segment not specified, generating random device segment";
          prefix = getRandomDeviceSegment(device_bits);
        }
        timestamp <<= device_bits;
        timestamp >>= device_bits;
        prefix = prefix + timestamp;
        logging::LOG_DEBUG(logger_) << "Using minifi uid prefix: " << std::hex << prefix;
      }
      for (int i = 0; i < 8; i++) {
        unsigned char prefix_element = (prefix >> ((7 - i) * 8)) & std::numeric_limits<unsigned char>::max();
        deterministic_prefix_[i] = prefix_element;
      }
      incrementor_ = 0;
    } else if (UUID_TIME_STR == implementation_str || UUID_WINDOWS_STR == implementation_str) {
      logging::LOG_DEBUG(logger_) << "Using uuid_generate_time implementation for uids.";
    } else {
      logging::LOG_DEBUG(logger_) << "Invalid value for uid.implementation (" << implementation_str << "). Using uuid_generate_time implementation for uids.";
    }
  } else {
    logging::LOG_DEBUG(logger_) << "Using uuid_generate_time implementation for uids.";
  }
}

#ifndef WIN32
bool IdGenerator::generateWithUuidImpl(unsigned int mode, Identifier::Data& output) {
  void* uuid = nullptr;
  try {
    std::lock_guard<std::mutex> lock(uuid_mutex_);
    uuid_impl_->make(mode);
    uuid = uuid_impl_->binary();
  } catch (uuid_error_t& uuid_error) {
    logger_->log_error("Failed to generate UUID, error: %s", uuid_error.string());
    return false;
  }

  memcpy(output.data(), uuid, 16);
  free(uuid);
  return true;
}
#endif

Identifier IdGenerator::generate() {
  Identifier::Data output{};
  switch (implementation_) {
    case UUID_RANDOM_IMPL:
    case UUID_DEFAULT_IMPL:
#ifdef WIN32
      windowsUuidGenerateRandom(output);
#else
      generateWithUuidImpl(UUID_MAKE_V4, output);
#endif
      break;
    case MINIFI_UID_IMPL: {
      std::memcpy(output.data(), deterministic_prefix_, sizeof(deterministic_prefix_));
      uint64_t incrementor_value = incrementor_++;
      for (int i = 8; i < 16; i++) {
        output[i] = (incrementor_value >> ((15 - i) * 8)) & std::numeric_limits<unsigned char>::max();
      }
    }
    break;
    case UUID_TIME_IMPL:
    default:
#ifdef WIN32
      windowsUuidGenerateTime(output);
#else
      generateWithUuidImpl(UUID_MAKE_V1, output);
#endif
      break;
  }
  return Identifier{output};
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
