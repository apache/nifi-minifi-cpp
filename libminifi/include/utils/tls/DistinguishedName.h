// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace org::apache::nifi::minifi::utils::tls {

class DistinguishedName {
 public:
  explicit DistinguishedName(const std::vector<std::string>& components);
  static DistinguishedName fromCommaSeparated(const std::string& comma_separated_components);
  static DistinguishedName fromSlashSeparated(const std::string& slash_separated_components);

  [[nodiscard]] std::optional<std::string> getCN() const;
  [[nodiscard]] std::string toString() const;

  friend bool operator==(const DistinguishedName& left, const DistinguishedName& right) { return left.components_ == right.components_; }
  friend bool operator!=(const DistinguishedName& left, const DistinguishedName& right) { return !(left == right); }

 private:
  std::vector<std::string> components_;
};

}  // namespace org::apache::nifi::minifi::utils::tls
