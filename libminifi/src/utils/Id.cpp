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
#include "utils/StringUtils.h"

#ifdef WIN32
#include "Rpc.h"
#include "Winsock2.h"
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#ifdef WIN32
namespace {
  void windowsUuidToUuidField(UUID* uuid, UUID_FIELD out) {
    uint32_t Data1BE = htonl(uuid->Data1);
    memcpy(out, &Data1BE, 4);
    uint16_t Data2BE = htons(uuid->Data2);
    memcpy(out + 4, &Data2BE, 2);
    uint16_t Data3BE = htons(uuid->Data3);
    memcpy(out + 6, &Data3BE, 2);
    memcpy(out + 8, uuid->Data4, 8);
  }

  void windowsUuidGenerateTime(UUID_FIELD out) {
    UUID uuid;
    UuidCreateSequential(&uuid);
    windowsUuidToUuidField(&uuid, out);
  }

  void windowsUuidGenerateRandom(UUID_FIELD out) {
    UUID uuid;
    UuidCreate(&uuid);
    windowsUuidToUuidField(&uuid, out);
  }
}  // namespace
#endif

Identifier::Identifier(UUID_FIELD u)
    : IdentifierBase(u) {
  build_string();
}

Identifier::Identifier()
    : IdentifierBase() {
}

Identifier::Identifier(const Identifier &other) {
  if (!other.convert().empty()) {
    copyInto(other);
    build_string();
  }
}

Identifier::Identifier(Identifier &&other)
    : IdentifierBase(std::move(other)) {
}

Identifier::Identifier(const IdentifierBase &other) {
  if (!other.convert().empty()) {
    copyInto(other);
    build_string();
  }
}

Identifier &Identifier::operator=(const Identifier &other) {
  if (!other.convert().empty()) {
    IdentifierBase::operator =(other);
    build_string();
  }
  return *this;
}

Identifier &Identifier::operator=(const IdentifierBase &other) {
  if (!other.convert().empty()) {
    IdentifierBase::operator =(other);
    build_string();
  }
  return *this;
}

Identifier &Identifier::operator=(UUID_FIELD o) {
  IdentifierBase::operator=(o);
  build_string();
  return *this;
}

Identifier &Identifier::operator=(std::string id) {
  sscanf(id.c_str(), "%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
         &id_[0], &id_[1], &id_[2], &id_[3],
         &id_[4], &id_[5],
         &id_[6], &id_[7],
         &id_[8], &id_[9],
         &id_[10], &id_[11], &id_[12], &id_[13], &id_[14], &id_[15]);
  build_string();
  return *this;
}

bool Identifier::operator==(const std::nullptr_t nullp) const {
  return converted_.empty();
}

bool Identifier::operator!=(const std::nullptr_t nullp) const {
  return !converted_.empty();
}

bool Identifier::operator!=(const Identifier &other) const {
  return converted_ != other.converted_;
}

bool Identifier::operator==(const Identifier &other) const {
  return converted_ == other.converted_;
}

std::string Identifier::to_string() const {
  return convert();
}

const unsigned char * const Identifier::toArray() const {
  return id_;
}

void Identifier::build_string() {
  char uuidStr[37];
  snprintf(uuidStr, sizeof(uuidStr), "%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
      id_[0], id_[1], id_[2], id_[3],
      id_[4], id_[5],
      id_[6], id_[7],
      id_[8], id_[9],
      id_[10], id_[11], id_[12], id_[13], id_[14], id_[15]);
  converted_ = uuidStr;
}

uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

NonRepeatingStringGenerator::NonRepeatingStringGenerator()
    : prefix_((std::to_string(timestamp) + "-")),
      incrementor_(0) {
}

IdGenerator::IdGenerator()
    : implementation_(UUID_TIME_IMPL),
      logger_(logging::LoggerFactory<IdGenerator>::getLogger()),
      incrementor_(0) {
#ifndef WIN32
  uuid_impl_ = std::unique_ptr<uuid>(new uuid());
#endif
}

IdGenerator::~IdGenerator() {
}

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
  UUID_FIELD random_uuid;
  for (int word = 0; word < 2; word++) {
#ifdef WIN32
    windowsUuidGenerateRandom(random_uuid);
#else
    uuid temp_uuid;
    temp_uuid.make(UUID_MAKE_V4);
    void* uuid_bin = temp_uuid.binary();
    memcpy(random_uuid, uuid_bin, 16);
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

void IdGenerator::initialize(const std::shared_ptr<Properties> & properties) {
  std::string implementation_str;
  implementation_ = UUID_TIME_IMPL;
  if (!properties->get("uid.implementation", implementation_str)) {
    logging::LOG_DEBUG(logger_) << "Using uuid_generate_time implementation for uids.";
    return;
  }
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
      if (properties->get("uid.minifi.device.segment", device_segment)) {
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
}

#ifndef WIN32
bool IdGenerator::generateWithUuidImpl(unsigned int mode, UUID_FIELD output) {
  void* uuid = nullptr;
  try {
    std::lock_guard<std::mutex> lock(uuid_mutex_);
    uuid_impl_->make(mode);
    uuid = uuid_impl_->binary();
  } catch (uuid_error_t& uuid_error) {
    logger_->log_error("Failed to generate UUID, error: %s", uuid_error.string());
    return false;
  }

  memcpy(output, uuid, 16);
  free(uuid);
  return true;
}
#endif

Identifier IdGenerator::generate() {
  Identifier ident;
  generate(ident);
  return ident;
}

void IdGenerator::generate(Identifier &ident) {
  UUID_FIELD output;
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
      const uint64_t incrementor_value = [this, &output] {
        std::lock_guard<std::mutex> lock{ uuid_mutex_ };
        std::memcpy(output, deterministic_prefix_, sizeof(deterministic_prefix_));
        return incrementor_++;
      }();
      for (unsigned int i = 8; i < 16; i++) {
        output[i] = (incrementor_value >> ((15 - i) * 8)) & std::numeric_limits<uint8_t>::max();
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
  ident = output;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
