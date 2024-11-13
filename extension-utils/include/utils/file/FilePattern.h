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
#include <utility>
#include <memory>
#include <filesystem>
#include <set>
#include <functional>

struct FilePatternTestAccessor;

namespace org::apache::nifi::minifi::utils::file {

class FilePatternError : public std::invalid_argument {
 public:
  explicit FilePatternError(const std::string& msg) : invalid_argument(msg) {}
};

class FilePattern {
  friend struct ::FilePatternTestAccessor;

  friend std::set<std::filesystem::path> match(const FilePattern& pattern);

  class FilePatternSegmentError : public std::invalid_argument {
   public:
    explicit FilePatternSegmentError(const std::string& msg) : invalid_argument(msg) {}
  };

  class FilePatternSegment {
   public:
    explicit FilePatternSegment(std::string pattern);

    enum class MatchResult {
      INCLUDE,  // dir/file should be processed according to the pattern
      EXCLUDE,  // dir/file is explicitly rejected by the pattern
      NOT_MATCHING  // dir/file does not match pattern, do what you may
    };

    [[nodiscard]] bool isExcluding() const {
      return excluding_;
    }

    [[nodiscard]] MatchResult matchDir(const std::filesystem::path& directory) const;

    [[nodiscard]] MatchResult matchFile(const std::filesystem::path& directory, const std::filesystem::path& filename) const;

    [[nodiscard]] MatchResult match(const std::filesystem::path& path) const;
    /**
     * @return The lowermost parent directory without wildcards.
     */
    [[nodiscard]]
    std::filesystem::path getBaseDirectory() const;

   private:
    enum class DirMatchResult {
      NONE,  // pattern does not match the directory (e.g. p = "/home/inner/*test", v = "/home/banana")
      PARENT,  // directory is a parent of the pattern (e.g. p = "/home/inner/*test", v = "/home/inner")
      EXACT,  // pattern exactly matches the directory (e.g. p = "/home/inner/*test", v = "/home/inner/cool_test")
      TREE  // pattern matches the whole subtree of the directory (e.g. p = "/home/**", v = "/home/banana")
    };

    using DirIt = std::filesystem::path::const_iterator;
    static DirMatchResult matchDirectory(DirIt pattern_it, const DirIt& pattern_end, DirIt value_it, const DirIt& value_end);

    std::filesystem::path directory_pattern_;
    std::string file_pattern_;
    bool excluding_ = false;
  };

  using ErrorHandler = std::function<void(std::string_view /*subpattern*/, std::string_view /*error_message*/)>;

  static void defaultErrorHandler(std::string_view subpattern, std::string_view error_message) {
    std::string message = "Error in subpattern '";
    message += subpattern;
    message += "': ";
    message += error_message;
    throw FilePatternError(message);
  }

 public:
  explicit FilePattern(const std::string& pattern, const ErrorHandler& error_handler = defaultErrorHandler);

 private:
  std::vector<FilePatternSegment> segments_;
};

std::set<std::filesystem::path> match(const FilePattern& pattern);

}  // namespace org::apache::nifi::minifi::utils::file
