/**
 *
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

#include "utils/RegexUtils.h"
#include "Exception.h"
#include <iostream>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

Regex::Regex() : Regex::Regex("") {}

Regex::Regex(const std::string &value) : Regex::Regex(value, {}) {}

Regex::Regex(const std::string &value,
                           const std::vector<Regex::Mode> &mode)
    : regexStr_(value),
      valid_(false) {
  if (regexStr_.empty())
    return;

  // Create regex mode
#ifdef NO_MORE_REGFREEE
  regex_mode_ = std::regex_constants::ECMAScript;
#else
  regex_mode_ = REG_EXTENDED;
#endif
  for (const auto m : mode) {
    switch (m) {
      case Mode::ICASE:
#ifdef NO_MORE_REGFREEE
        regex_mode_ |= std::regex_constants::icase;
#else
        regex_mode_ |= REG_ICASE;
#endif
        break;
    }
  }
  // Compile
#ifdef NO_MORE_REGFREEE
  try {
    compiledRegex_ = std::regex(regexStr_, regex_mode_);
    valid_ = true;
  } catch (const std::regex_error &e) {
    throw Exception(REGEX_EXCEPTION, e.what());
  }
#else
  int err_code = regcomp(&compiledRegex_, regexStr_.c_str(), regex_mode_);
  if (err_code) {
    const size_t sz = regerror(err_code, &compiledRegex_, nullptr, 0);
    std::vector<char> msg(sz);
    regerror(err_code, &compiledRegex_, msg.data(), msg.size());
    throw Exception(REGEX_EXCEPTION, std::string(msg.begin(), msg.end()));
  }
  valid_ = true;
  int maxGroups = std::count(regexStr_.begin(), regexStr_.end(), '(') + 1;
  matches_.resize(maxGroups);
#endif
}

Regex::Regex(Regex&& other)
#ifndef NO_MORE_REGFREEE
  : valid_(false),
    regex_mode_(REG_EXTENDED)
#endif
{
  *this = std::move(other);
}

Regex& Regex::operator=(Regex&& other) {
  if (this == &other) {
    return *this;
  }

  pat_ = std::move(other.pat_);
  suffix_ = std::move(other.suffix_);
  regexStr_ = std::move(other.regexStr_);
  results_ = std::move(other.results_);
#ifdef NO_MORE_REGFREEE
  compiledRegex_ = std::move(other.compiledRegex_);
  regex_mode_ = other.regex_mode_;
  matches_ = std::move(other.matches_);
#else
  if (valid_)
    regfree(&compiledRegex_);
  compiledRegex_ = other.compiledRegex_;
  regex_mode_ = other.regex_mode_;
  matches_ = std::move(other.matches_);
#endif
  valid_ = other.valid_;
  other.valid_ = false;
  return *this;
}

Regex::~Regex() {
#ifndef NO_MORE_REGFREEE
  if (valid_)
    regfree(&compiledRegex_);
#endif
}

bool Regex::match(const std::string &pattern) {
  if (!valid_) {
    return false;
  }
  results_.clear();
  pat_ = pattern;
#ifdef NO_MORE_REGFREEE
  if (std::regex_search(pattern, matches_, compiledRegex_)) {
    for (const auto &m : matches_) {
      results_.push_back(m.str());
    }
    suffix_ = matches_.suffix();
    return true;
  }
  return false;
#else
  if (regexec(&compiledRegex_, pattern.c_str(), matches_.size(),
              matches_.data(), 0) == 0) {
    for (const auto &m : matches_) {
      if (m.rm_so == -1) {
        break;
      }
      std::string s(pattern.begin() + m.rm_so, pattern.begin() + m.rm_eo);
      results_.push_back(s);
    }
    if ((size_t) matches_[0].rm_eo >= pattern.size()) {
      suffix_ = "";
    } else {
      suffix_ = pattern.substr(matches_[0].rm_eo);
    }
    return true;
  }
  return false;
#endif
}

const std::vector<std::string>& Regex::getResult() const { return results_; }

const std::string& Regex::getSuffix() const { return suffix_; }

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
