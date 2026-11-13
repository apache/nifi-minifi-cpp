// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "utils/HTTPUtils.h"

#include <array>
#include <filesystem>

namespace org::apache::nifi::minifi::utils {

std::optional<std::string_view> getDefaultCAFile() {
#ifndef WIN32
  static constexpr std::array<std::string_view, 5> possible_ca_paths = {
      "/usr/local/share/certs/ca-root-nss.crt",
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/usr/share/ssl/certs/ca-bundle.crt",
      "/etc/ssl/cert.pem"
  };

  for (const auto& possible_ca_path : possible_ca_paths) {
    if (std::filesystem::exists(possible_ca_path)) {
      return possible_ca_path;
    }
  }
#endif
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils
