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
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <uuid/uuid.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

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
  uuid_parse(id.c_str(), id_);
  converted_ = id;
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
  char uuidStr[37] = { 0 };
  uuid_unparse_lower(id_, uuidStr);
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
    uuid_generate_random(random_uuid);
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
  if (properties->get("uid.implementation", implementation_str)) {
    std::transform(implementation_str.begin(), implementation_str.end(), implementation_str.begin(), ::tolower);
    if ("random" == implementation_str) {
      logging::LOG_DEBUG(logger_) << "Using uuid_generate_random for uids.";
      implementation_ = UUID_RANDOM_IMPL;
    } else if ("uuid_default" == implementation_str) {
      logging::LOG_DEBUG(logger_) << "Using uuid_generate for uids.";
      implementation_ = UUID_DEFAULT_IMPL;
    } else if ("minifi_uid" == implementation_str) {
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
        unsigned char prefix_element = (prefix >> ((7 - i) * 8)) & UNSIGNED_CHAR_MAX;
        deterministic_prefix_[i] = prefix_element;
      }
      incrementor_ = 0;
    } else if ("time" == implementation_str) {
      logging::LOG_DEBUG(logger_) << "Using uuid_generate_time implementation for uids.";
    } else {
      logging::LOG_DEBUG(logger_) << "Invalid value for uid.implementation (" << implementation_str << "). Using uuid_generate_time implementation for uids.";
    }
  } else {
    logging::LOG_DEBUG(logger_) << "Using uuid_generate_time implementation for uids.";
  }
}

Identifier IdGenerator::generate() {
  Identifier ident;
  generate(ident);
  return ident;
}

void IdGenerator::generate(Identifier &ident) {
  UUID_FIELD output;
  switch (implementation_) {
    case UUID_RANDOM_IMPL:
      uuid_generate_random(output);
      break;
    case UUID_DEFAULT_IMPL:
      uuid_generate(output);
      break;
    case MINIFI_UID_IMPL: {
      std::memcpy(output, deterministic_prefix_, sizeof(deterministic_prefix_));
      uint64_t incrementor_value = incrementor_++;
      for (int i = 8; i < 16; i++) {
        output[i] = (incrementor_value >> ((15 - i) * 8)) & UNSIGNED_CHAR_MAX;
      }
    }
      break;
    default:
      uuid_generate_time(output);
      break;
  }
  ident = output;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
