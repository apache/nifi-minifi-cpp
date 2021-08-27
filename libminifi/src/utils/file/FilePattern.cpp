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

#include "utils/file/FilePattern.h"
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

static bool isGlobPattern(const std::string& pattern) {
  return pattern.find_first_of("?*") != std::string::npos;
}

FilePattern::FilePatternSegment::FilePatternSegment(std::string pattern) {
  pattern = utils::StringUtils::trim(pattern);
  excluding_ = false;
  if (!pattern.empty() && pattern[0] == '!') {
    excluding_ = true;
    pattern = utils::StringUtils::trim(pattern.substr(1));
  }
  if (pattern.empty()) {
    throw FilePatternSegmentError("Empty pattern");
  }
  std::filesystem::path exe_dir = get_executable_dir();
  std::filesystem::path path = pattern;
  if (!exe_dir.is_absolute() && path.is_relative()) {
    throw FilePatternSegmentError("Couldn't determine executable dir, relative pattern not supported");
  }
  path = exe_dir / path;
  file_pattern_ = path.filename();
  if (file_pattern_.empty()) {
    throw FilePatternSegmentError("Empty file pattern");
  }
  if (file_pattern_ == "**") {
    file_pattern_ = "*";
    // include the "**" in the directory pattern
    directory_pattern_ = path;
  } else {
    directory_pattern_ = path.parent_path();
  }
  if (file_pattern_ == "." || file_pattern_ == "..") {
    throw FilePatternSegmentError("Invalid file pattern '" + file_pattern_ + "'");
  }
  bool after_wildcard = false;
  for (const auto& segment : directory_pattern_) {
    if (after_wildcard && segment == "..") {
      throw FilePatternSegmentError("Parent accessor is not supported after wildcards");
    }
    if (isGlobPattern(segment.string())) {
      after_wildcard = true;
    }
  }
}

std::filesystem::path FilePattern::FilePatternSegment::getBaseDirectory() const {
  std::filesystem::path base_dir;
  for (const auto& segment : directory_pattern_) {
    // ignore segments at or after wildcards
    if (isGlobPattern(segment.string())) {
      break;
    }
    base_dir /= segment;
  }
  return base_dir;
}

FilePattern::FilePattern(const std::string &pattern, ErrorHandler error_handler) {
  for (const auto& segment : StringUtils::split(pattern, ",")) {
    try {
      segments_.push_back(FilePatternSegment(segment));
    } catch (const FilePatternSegmentError& segment_error) {
      error_handler(segment, segment_error.what());
    }
  }
}

template<typename It>
static bool advance_if_not_equal(It& it, const It& end) {
  if (it == end) {
    return false;
  }
  ++it;
  return true;
}

static bool is_this_dir(const std::filesystem::path& dir) {
  return dir.empty() || dir == ".";
}

template<typename It, typename Fn>
static void skip_if(It& it, const It& end, const Fn& fn) {
  while (it != end && fn(*it)) {
    ++it;
  }
}

static bool matchGlob(std::string_view pattern, std::string_view value) {
  // match * and ?
  size_t value_idx = 0;
  for (size_t pattern_idx = 0; pattern_idx != pattern.length(); ++pattern_idx) {
    if (pattern[pattern_idx] == '*') {
      do {
        if (matchGlob(pattern.substr(pattern_idx + 1), value.substr(value_idx))) {
          return true;
        }
      } while (advance_if_not_equal(value_idx, value.length()));
      return false;
    }
    if (value_idx == value.length()) {
      return false;
    }
    if (pattern[pattern_idx] != '?' && pattern[pattern_idx] != value[value_idx]) {
      return false;
    }
    ++value_idx;
  }
  return value_idx == value.length();
}

auto FilePattern::FilePatternSegment::matchDirectory(DirIt pattern_it, DirIt pattern_end, DirIt value_it, DirIt value_end) -> DirMatchResult {
  for (; pattern_it != pattern_end; ++pattern_it) {
    if (is_this_dir(*pattern_it)) {
      continue;
    }
    if (*pattern_it == "**") {
      if (std::next(pattern_it) == pattern_end) {
        return DirMatchResult::TREE;
      }
      bool matched_parent = false;
      // any number of nested directories
      do {
        skip_if(value_it, value_end, is_this_dir);
        auto result = matchDirectory(std::next(pattern_it), pattern_end, value_it, value_end);
        if (result == DirMatchResult::TREE || result == DirMatchResult::EXACT) {
          return result;
        }
        if (result == DirMatchResult::PARENT) {
          // even though we have a parent match, there may be a "better" (exact, tree) match
          matched_parent = true;
        }
      } while (advance_if_not_equal(value_it, value_end));
      if (matched_parent) {
        return DirMatchResult::PARENT;
      }
      return DirMatchResult::NONE;
    }
    skip_if(value_it, value_end, is_this_dir);
    if (value_it == value_end) {
      // we used up all the value segments but there are still pattern segments
      return DirMatchResult::PARENT;
    }
    if (!matchGlob(pattern_it->string(), value_it->string())) {
      return DirMatchResult::NONE;
    }
    ++value_it;
  }
  skip_if(value_it, value_end, is_this_dir);
  if (value_it == value_end) {
    // used up all pattern and value segments
    return DirMatchResult::EXACT;
  } else {
    // used up all pattern segments but we still have value segments
    return DirMatchResult::NONE;
  }
}

auto FilePattern::FilePatternSegment::match(const std::string& directory) const -> MatchResult {
  std::filesystem::path value = directory;
  auto result = matchDirectory(directory_pattern_.begin(), directory_pattern_.end(), value.begin(), value.end());
  if (excluding_) {
    if (result == DirMatchResult::TREE && file_pattern_ == "*") {
      // all files are excluded in this directory
      return MatchResult::EXCLUDE;
    }
    return MatchResult::NOT_MATCHING;
  }
  return result != DirMatchResult::NONE ? MatchResult::INCLUDE : MatchResult::NOT_MATCHING;
}

auto FilePattern::FilePatternSegment::match(const std::string& directory, const std::string& filename) const -> MatchResult {
  std::filesystem::path value = directory;
  auto result = matchDirectory(directory_pattern_.begin(), directory_pattern_.end(), value.begin(), value.end());
  if (result != DirMatchResult::EXACT && result != DirMatchResult::TREE) {
    // we only match a file if the directory fully matches
    return MatchResult::NOT_MATCHING;
  }
  if (matchGlob(file_pattern_, filename)) {
    return excluding_ ? MatchResult::EXCLUDE : MatchResult::INCLUDE;
  }
  return MatchResult::NOT_MATCHING;
}

auto FilePattern::FilePatternSegment::match(const std::filesystem::path& path) const -> MatchResult {
  if (path.has_filename()) {
    return match(path.parent_path().string(), path.filename());
  }
  return match(path.parent_path().string());
}

static std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<FilePattern>::getLogger();

std::set<std::filesystem::path> match(const FilePattern& pattern) {
  using FilePatternSegment = FilePattern::FilePatternSegment;
  std::set<std::filesystem::path> files;
  for (auto it = pattern.segments_.begin(); it != pattern.segments_.end(); ++it) {
    if (it->isExcluding()) continue;
    const auto match_file = [&] (const std::string& dir, const std::string& file) -> bool {
      if (it->match(dir, file) != FilePatternSegment::MatchResult::INCLUDE) {
        // our main pattern does not explicitly command us to process this file
        // keep iterating
        return true;
      }
      // check all subsequent patterns in reverse (later ones have higher precedence)
      for (auto rit = pattern.segments_.rbegin(); rit.base() != it + 1; ++rit) {
        const auto result = rit->match(dir, file);
        if (result == FilePatternSegment::MatchResult::INCLUDE) {
          break;
        } else if (result == FilePatternSegment::MatchResult::EXCLUDE) {
          // keep on processing the rest of the files in the current directory
          return true;
        }
      }
      files.insert(concat_path(dir, file));
      return true;
    };
    const auto descend_into_directory = [&] (const std::string& dir) -> bool {
      if (it->match(dir) != FilePatternSegment::MatchResult::INCLUDE) {
        // our main pattern does not explicitly command us to process this directory
        // do not descend into this directory
        return false;
      }
      // check all subsequent patterns in reverse (later ones have higher precedence)
      for (auto rit = pattern.segments_.rbegin(); rit.base() != it + 1; ++rit) {
        const auto result = rit->match(dir);
        if (result == FilePatternSegment::MatchResult::INCLUDE) {
          break;
        } else if (result == FilePatternSegment::MatchResult::EXCLUDE) {
          // do not descend into this directory
          return false;
        }
      }
      return true;
    };
    list_dir(it->getBaseDirectory(), match_file, logger, descend_into_directory);
  }
  return files;
}

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
