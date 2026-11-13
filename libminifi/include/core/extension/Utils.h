// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "minifi-cpp/utils/gsl.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::core::extension::internal {

template<typename Callback>
class Timer {
 public:
  explicit Timer(Callback cb): cb_(std::move(cb)) {}
  ~Timer() {
    cb_(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_));
  }
 private:
  std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
  Callback cb_;
};

struct LibraryDescriptor {
  std::string name;
  std::filesystem::path dir;
  std::string filename;

  [[nodiscard]]
  std::filesystem::path getFullPath() const {
    return dir / filename;
  }
};

inline std::optional<LibraryDescriptor> asDynamicLibrary(const std::filesystem::path& path) {
#if defined(WIN32)
  static const std::string_view extension = ".dll";
#elif defined(__APPLE__)
  static const std::string_view extension = ".dylib";
#else
  static const std::string_view extension = ".so";
#endif

#ifdef WIN32
  static const std::string_view prefix = "";
#else
  static const std::string_view prefix = "lib";
#endif
  if (!path.filename().string().starts_with(prefix) || path.filename().extension().string() != extension) {
    return {};
  }
  std::string filename = path.filename().string();
  return LibraryDescriptor{
      filename.substr(prefix.length(), filename.length() - extension.length() - prefix.length()),
      path.parent_path(),
      filename
  };
}

}  // namespace org::apache::nifi::minifi::core::extension::internal
