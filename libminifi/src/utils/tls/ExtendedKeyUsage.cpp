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
#include "utils/tls/ExtendedKeyUsage.h"

#include <openssl/x509v3.h>

#include <array>
#include <cassert>

#include "core/logging/LoggerFactory.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::tls {

namespace {

struct KeyValuePair {
  const char* key;
  uint8_t value;
};
// see https://tools.ietf.org/html/rfc5280#section-4.2.1.12
// note that the chosen bit position is the last byte of the OID
constexpr std::array<KeyValuePair, 6> EXT_KEY_USAGE_NAME_TO_BIT_POS{{
    KeyValuePair{"Server Authentication", 1},
    KeyValuePair{"Client Authentication", 2},
    KeyValuePair{"Code Signing", 3},
    KeyValuePair{"Secure Email", 4},
    KeyValuePair{"Time Stamping", 8},
    KeyValuePair{"OCSP Signing", 9}
}};

}  // namespace

void EXTENDED_KEY_USAGE_deleter::operator()(EXTENDED_KEY_USAGE* key_usage) const { EXTENDED_KEY_USAGE_free(key_usage); }

ExtendedKeyUsage::ExtendedKeyUsage() : logger_(core::logging::LoggerFactory<ExtendedKeyUsage>::getLogger()) {}

ExtendedKeyUsage::ExtendedKeyUsage(const EXTENDED_KEY_USAGE& key_usage_asn1) : ExtendedKeyUsage{} {
  const int num_oids = sk_ASN1_OBJECT_num(&key_usage_asn1);
  for (int i = 0; i < num_oids; ++i) {
    const ASN1_OBJECT* const oid = sk_ASN1_OBJECT_value(&key_usage_asn1, i);
    assert(oid);
    auto length = OBJ_length(oid);
    assert(length > 0);
    auto data = OBJ_get0_data(oid);
    const unsigned char last_byte_of_oid = data[length - 1];
    if (last_byte_of_oid < bits_.size()) {
      bits_.set(last_byte_of_oid);
    }
  }
}

ExtendedKeyUsage::ExtendedKeyUsage(const std::string& key_usage_str) : ExtendedKeyUsage{} {
  const std::vector<std::string> key_usages = utils::string::split(key_usage_str, ",");
  for (const auto& key_usage : key_usages) {
    const std::string key_usage_trimmed = utils::string::trim(key_usage);
    const auto it = std::find_if(EXT_KEY_USAGE_NAME_TO_BIT_POS.begin(), EXT_KEY_USAGE_NAME_TO_BIT_POS.end(),
                                 [key_usage_trimmed](const KeyValuePair& kv){ return kv.key == key_usage_trimmed; });
    if (it != EXT_KEY_USAGE_NAME_TO_BIT_POS.end()) {
      const uint8_t bit_pos = it->value;
      bits_.set(bit_pos);
    } else {
      logger_->log_error("Ignoring unrecognized extended key usage type {}", key_usage_trimmed);
    }
  }
}

bool ExtendedKeyUsage::isSubsetOf(const ExtendedKeyUsage& other) const {
  return (bits_ & other.bits_) == bits_;
}

bool operator==(const ExtendedKeyUsage& left, const ExtendedKeyUsage& right) {
  return left.bits_ == right.bits_;
}

bool operator!=(const ExtendedKeyUsage& left, const ExtendedKeyUsage& right) {
  return !(left == right);
}

}  // namespace org::apache::nifi::minifi::utils::tls
