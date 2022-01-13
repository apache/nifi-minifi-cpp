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
#ifdef OPENSSL_SUPPORT

#include <bitset>
#include <cinttypes>
#include <memory>
#include <string>

#include "core/logging/Logger.h"

struct stack_st_ASN1_OBJECT;
typedef stack_st_ASN1_OBJECT EXTENDED_KEY_USAGE;

namespace org::apache::nifi::minifi::utils::tls {

struct EXTENDED_KEY_USAGE_deleter {
  void operator()(EXTENDED_KEY_USAGE* key_usage) const;
};
using EXTENDED_KEY_USAGE_unique_ptr = std::unique_ptr<EXTENDED_KEY_USAGE, EXTENDED_KEY_USAGE_deleter>;

class ExtendedKeyUsage {
 public:
  ExtendedKeyUsage();
  explicit ExtendedKeyUsage(const EXTENDED_KEY_USAGE& key_usage_asn1);
  explicit ExtendedKeyUsage(const std::string& key_usage_str);

  bool isSubsetOf(const ExtendedKeyUsage& other) const;

  friend bool operator==(const ExtendedKeyUsage& left, const ExtendedKeyUsage& right);
  friend bool operator!=(const ExtendedKeyUsage& left, const ExtendedKeyUsage& right);

 private:
  std::bitset<16> bits_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils::tls

#endif  // OPENSSL_SUPPORT
