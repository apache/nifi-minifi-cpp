// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <bitset>
#include <cinttypes>
#include <memory>
#include <string>

#include "minifi-cpp/core/logging/Logger.h"

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
