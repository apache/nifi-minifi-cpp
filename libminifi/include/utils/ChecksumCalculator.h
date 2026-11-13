// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace org::apache::nifi::minifi::utils {

class ChecksumCalculator {
 public:
  static constexpr const char* CHECKSUM_TYPE = "SHA256";
  static constexpr size_t LENGTH_OF_HASH_IN_BYTES = 32;

  void setFileLocations(std::vector<std::filesystem::path> file_locations);
  [[nodiscard]] std::filesystem::path getFileNameOfFirstFileLocation() const;
  std::string getChecksum();
  void invalidateChecksum();

 private:
  static std::string computeChecksum(const std::vector<std::filesystem::path>& file_locations);

  std::vector<std::filesystem::path> file_locations_;
  std::optional<std::string> checksum_;
};

inline void ChecksumCalculator::invalidateChecksum() {
  checksum_.reset();
}

}  // namespace org::apache::nifi::minifi::utils
