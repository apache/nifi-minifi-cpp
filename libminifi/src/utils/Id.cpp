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

uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

NonRepeatingStringGenerator::NonRepeatingStringGenerator()
    : prefix_((std::to_string(timestamp) + "-")) {
}

IdGenerator::IdGenerator()
    : implementation_(UUID_TIME_IMPL),
      logger_(logging::LoggerFactory<IdGenerator>::getLogger()),
      incrementor_(0) {
}

uint64_t IdGenerator::getDeviceSegmentFromString(const std::string& str, int numBits) {
  uint64_t deviceSegment = 0;
  for (int i = 0; i < str.length(); i++) {
    unsigned char c = toupper(str[i]);
    if (c >= '0' && c <= '9') {
      deviceSegment = deviceSegment + (c - '0');
    } else if (c >= 'A' && c <= 'F') {
      deviceSegment = deviceSegment + (c - 'A' + 10);
    } else {
      logger_->log_error("Expected hex char (0-9, A-F).  Got %c.", c);
    }
    deviceSegment = deviceSegment << 4;
  }
  deviceSegment <<= 64 - (4 * (str.length() + 1));
  deviceSegment >>= 64 - numBits;
  logger_->log_debug("Using user defined device segment: %" PRIx64, deviceSegment);
  deviceSegment <<= 64 - numBits;
  return deviceSegment;
}

uint64_t IdGenerator::getRandomDeviceSegment(int numBits) {
  uint64_t deviceSegment = 0;
  uuid_t random_uuid;
  for (int word = 0; word < 2; word++) {
    uuid_generate_random(random_uuid);
    for (int i = 0; i < 4; i++) {
      deviceSegment += random_uuid[i];
      deviceSegment <<= 8;
    }
  }
  deviceSegment >>= 64 - numBits;
  logger_->log_debug("Using random device segment: %" PRIx64, deviceSegment);
  deviceSegment <<= 64 - numBits;
  return deviceSegment;
}

void IdGenerator::initialize(const std::shared_ptr<Properties> & properties) {
  std::string implementation_str;
  implementation_ = UUID_TIME_IMPL;
  if (properties->get("uid.implementation", implementation_str)) {
    std::transform(implementation_str.begin(), implementation_str.end(), implementation_str.begin(), ::tolower);
    if ("random" == implementation_str) {
      logger_->log_debug("Using uuid_generate_random for uids.");
      implementation_ = UUID_RANDOM_IMPL;
    } else if ("uuid_default" == implementation_str) {
      logger_->log_debug("Using uuid_generate for uids.");
      implementation_ = UUID_DEFAULT_IMPL;
    } else if ("minifi_uid" == implementation_str) {
      logger_->log_debug("Using minifi uid implementation for uids");
      implementation_ = MINIFI_UID_IMPL;

      uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      int device_bits = properties->getInt("uid.minifi.device.segment.bits", 16);
      std::string device_segment;
      uint64_t prefix = timestamp;
      if (device_bits > 0) {
        if (properties->get("uid.minifi.device.segment", device_segment)) {
          prefix = getDeviceSegmentFromString(device_segment, device_bits);
        } else {
          logger_->log_warn("uid.minifi.device.segment not specified, generating random device segment");
          prefix = getRandomDeviceSegment(device_bits);
        }
        timestamp <<= device_bits;
        timestamp >>= device_bits;
        prefix = prefix + timestamp;
        logger_->log_debug("Using minifi uid prefix: %16" PRIx64, prefix);
      }
      for (int i = 0; i < 8; i++) {
        unsigned char prefix_element = (prefix >> ((7 - i) * 8)) & UNSIGNED_CHAR_MAX;
        deterministic_prefix_[i] = prefix_element;
      }
      incrementor_ = 0;
    } else if ("time" == implementation_str) {
      logger_->log_debug("Using uuid_generate_time implementation for uids.");
    } else {
      logger_->log_debug("Invalid value for uid.implementation (%s). Using uuid_generate_time implementation for uids.", implementation_str);
    }
  } else {
    logger_->log_debug("Using uuid_generate_time implementation for uids.");
  }
}

void IdGenerator::generate(uuid_t output) {
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
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
