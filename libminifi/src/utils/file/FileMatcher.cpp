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

#include "utils/file/FileMatcher.h"
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

std::shared_ptr<core::logging::Logger> FileMatcher::FilePattern::logger_ = logging::LoggerFactory<FileMatcher::FilePattern>::getLogger();
std::shared_ptr<core::logging::Logger> FileMatcher::logger_ = logging::LoggerFactory<FileMatcher>::getLogger();

static bool isGlobPattern(const std::string& pattern) {
  return pattern.find_first_of("?*") != std::string::npos;
}

static std::vector<std::string> split(const std::string& str, const std::vector<std::string>& delimiters) {
  std::vector<std::string> result;

  size_t prev_delim_end = 0;
  size_t next_delim_begin = std::string::npos;
  do {
    for (const auto& delim : delimiters) {
      next_delim_begin = str.find(delim, prev_delim_end);
      if (next_delim_begin != std::string::npos) {
        result.push_back(str.substr(prev_delim_end, next_delim_begin - prev_delim_end));
        prev_delim_end = next_delim_begin + delim.length();
        break;
      }
    }
  } while (next_delim_begin != std::string::npos);
  result.push_back(str.substr(prev_delim_end));
  return result;
}

optional<FileMatcher::FilePattern> FileMatcher::FilePattern::fromPattern(std::string pattern) {
  pattern = utils::StringUtils::trim(pattern);
  bool excluding = false;
  if (!pattern.empty() && pattern[0] == '!') {
    excluding = true;
    pattern = utils::StringUtils::trim(pattern.substr(1));
  }
  if (pattern.empty()) {
    logger_->log_error("Empty pattern");
    return nullopt;
  }
  std::string exe_dir = get_executable_dir();
  if (exe_dir.empty() && !isAbsolutePath(pattern.c_str())) {
    logger_->log_error("Couldn't determine executable dir, relative pattern '%s' not supported", pattern);
    return nullopt;
  }
  pattern = resolve(exe_dir, pattern);
  auto segments = split(pattern, {"/", "\\"});
  gsl_Expects(!segments.empty());
  auto file_pattern = segments.back();
  if (file_pattern == "**") {
    file_pattern = "*";
  } else {
    segments.pop_back();
  }
  if (file_pattern == "." || file_pattern == "..") {
    logger_->log_error("Invalid file pattern '%s'", file_pattern);
    return nullopt;
  }
  return FilePattern(segments, file_pattern, excluding);
}

std::string FileMatcher::FilePattern::getBaseDirectory() const {
  std::string base_dir;
  for (const auto& segment : directory_segments_) {
    // ignore segments at or after wildcards
    if (isGlobPattern(segment)) {
      break;
    }
    base_dir += segment + get_separator();
  }
  return base_dir;
}

FileMatcher::FileMatcher(const std::string &patterns) {
  for (auto&& pattern : split(patterns, {","})) {
    if (auto&& p = FilePattern::fromPattern(pattern)) {
      patterns_.push_back(std::move(p.value()));
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

static bool is_this_dir(const std::string& dir) {
  return dir.empty() || dir == ".";
}

template<typename It, typename Fn>
static void skip_if(It& it, const It& end, const Fn& fn) {
  while (it != end && fn(*it)) {
    ++it;
  }
}

static bool matchGlob(std::string::const_iterator pattern_it, std::string::const_iterator pattern_end, std::string::const_iterator value_it, std::string::const_iterator value_end) {
  // match * and ?
  for (; pattern_it != pattern_end; ++pattern_it) {
    if (*pattern_it == '*') {
      do {
        if (matchGlob(std::next(pattern_it), pattern_end, value_it, value_end)) {
          return true;
        }
      } while (advance_if_not_equal(value_it, value_end));
      return false;
    }
    if (value_it == value_end) {
      return false;
    }
    if (*pattern_it != '?' && *pattern_it != *value_it) {
      return false;
    }
    ++value_it;
  }
  return value_it == value_end;
}

FileMatcher::FilePattern::DirMatchResult FileMatcher::FilePattern::matchDirectory(DirIt pattern_it, DirIt pattern_end, DirIt value_it, DirIt value_end) {
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
    if (!matchGlob(pattern_it->begin(), pattern_it->end(), value_it->begin(), value_it->end())) {
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

bool FileMatcher::FilePattern::match(const std::string& directory, const optional<std::string>& filename) const {
  auto value = split(directory, {"/", "\\"});
  auto result = matchDirectory(directory_segments_.begin(), directory_segments_.end(), value.begin(), value.end());
  if (!filename) {
    if (excluding_) {
      if (result == DirMatchResult::TREE && file_pattern_ == "*") {
        // all files are excluded in this directory
        return true;
      }
      return false;
    }
    return result != DirMatchResult::NONE;
  }
  if (result != DirMatchResult::EXACT && result != DirMatchResult::TREE) {
    // we only accept a file if the directory fully matches
    return false;
  }
  return matchGlob(file_pattern_.begin(), file_pattern_.end(), filename->begin(), filename->end());
}

void FileMatcher::forEachFile(const std::function<bool(const std::string&, const std::string&)>& fn) const {
  bool terminate = false;
  std::set<std::string> files;
  for (auto it = patterns_.begin(); it != patterns_.end(); ++it) {
    if (it->isExcluding()) continue;
    std::function<bool(const std::string&, const utils::optional<std::string>&)> matcher = [&] (const std::string& dir, const utils::optional<std::string>& file) -> bool {
      if (terminate) return false;
      if (!it->match(dir, file)) {
        if (file) {
          // keep iterating
          return true;
        }
        // do not descend into this directory
        return false;
      }
      // check all subsequent patterns in reverse (later ones have higher precedence)
      for (auto rit = patterns_.rbegin(); rit.base() != it + 1; ++rit) {
        if (rit->match(dir, file)) {
          if (rit->isExcluding()) {
            if (file) {
              // keep on iterating if this is a file
              return true;
            }
            // do not descend into this directory
            return false;
          }
          break;
        }
      }
      if (file) {
        if (files.insert(concat_path(dir, file.value())).second) {
          // this is a new file, we haven't checked before
          if (!fn(dir, file.value())) {
            terminate = true;
            return false;
          }
        }
      }
      return true;
    };
    list_dir(it->getBaseDirectory(), matcher, logger_);
  }
}

std::set<std::string> FileMatcher::listFiles() const {
  std::set<std::string> files;
  forEachFile([&] (const std::string& dir, const std::string& filename) {
    files.insert(concat_path(dir, filename));
    return true;
  });
  return files;
}

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
