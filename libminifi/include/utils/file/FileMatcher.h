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

#include <string>
#include <vector>
#include <set>
#include <utility>
#include <memory>

#include "utils/OptionalUtils.h"
#include "core/logging/Logger.h"

struct FileMatcherTestAccessor;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

class FileMatcher {
  friend struct ::FileMatcherTestAccessor;
  class FilePattern {
    FilePattern(std::vector<std::string> directory_segments, std::string file_pattern, bool excluding)
      : directory_segments_(std::move(directory_segments)),
        file_pattern_(std::move(file_pattern)),
        excluding_(excluding) {}

   public:
    static optional<FilePattern> fromPattern(std::string pattern, bool log_errors = true);

    bool isExcluding() const {
      return excluding_;
    }

    bool match(const std::string& directory, const optional<std::string>& filename = {}) const;

    /**
     * @return The lowermost parent directory without wildcards.
     */
    std::string getBaseDirectory() const;

   private:
    enum class DirMatchResult {
      NONE,  // pattern does not match the directory (e.g. p = "/home/inner/*test", v = "/home/banana")
      PARENT,  // directory is a parent of the pattern (e.g. p = "/home/inner/*test", v = "/home/inner")
      EXACT,  // pattern exactly matches the directory (e.g. p = "/home/inner/*test", v = "/home/inner/cool_test")
      TREE  // pattern matches the whole subtree of the directory (e.g. p = "/home/**", v = "/home/banana")
    };

    using DirIt = std::vector<std::string>::const_iterator;
    static DirMatchResult matchDirectory(DirIt pattern_begin, DirIt pattern_end, DirIt value_begin, DirIt value_end);

    std::vector<std::string> directory_segments_;
    std::string file_pattern_;
    bool excluding_;

    static std::shared_ptr<core::logging::Logger> logger_;
  };

 public:
  explicit FileMatcher(const std::string& patterns);

  std::set<std::string> listFiles() const;
  void forEachFile(const std::function<bool(const std::string&, const std::string&)>& fn) const;

 private:
  std::vector<FilePattern> patterns_;

  static std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
