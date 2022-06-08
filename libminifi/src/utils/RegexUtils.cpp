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

#include <iostream>
#include <vector>

#include "Exception.h"

#ifndef NO_MORE_REGFREEE
namespace {

std::size_t getMaxGroupCountOfRegex(const std::string& regex) {
  return std::count(regex.begin(), regex.end(), '(') + 1;
}

}  // namespace
#endif

namespace org::apache::nifi::minifi::utils {

#ifndef NO_MORE_REGFREEE
SMatch::SuffixWrapper SMatch::suffix() const {
  if ((size_t) matches_[0].match.rm_eo >= string_.size()) {
    return SuffixWrapper{std::string()};
  } else {
    return SuffixWrapper{string_.substr(matches_[0].match.rm_eo)};
  }
}

const SMatch::Regmatch& SMatch::operator[](std::size_t index) const {
  return matches_[index];
}

std::size_t SMatch::size() const {
  std::size_t count = 0;
  for (const auto &m : matches_) {
    if (m.match.rm_so == -1) {
      break;
    }
    ++count;
  }
  return count;
}

bool SMatch::ready() const {
  return !matches_.empty();
}

std::size_t SMatch::position(std::size_t index) const {
  return matches_.at(index).match.rm_so;
}

std::size_t SMatch::length(std::size_t index) const {
  return matches_.at(index).match.rm_eo - matches_.at(index).match.rm_so;
}

void SMatch::clear() {
  matches_.clear();
  string_.clear();
}
#endif

Regex::Regex() : Regex::Regex("") {}

Regex::Regex(const std::string &value) : Regex::Regex(value, {}) {}

Regex::Regex(const std::string &value,
             const std::vector<Regex::Mode> &mode)
    : regex_str_(value),
      valid_(false) {
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
#ifdef NO_MORE_REGFREEE
  try {
    compiled_regex_ = std::regex(regex_str_, regex_mode_);
    valid_ = true;
  } catch (const std::regex_error &e) {
    throw Exception(REGEX_EXCEPTION, e.what());
  }
#else
  compileRegex(compiled_regex_, regex_str_);
  compileRegex(compiled_full_input_regex_, '^' + regex_str_ + '$');
  valid_ = true;
#endif
}

Regex::Regex(const Regex& other)
#ifndef NO_MORE_REGFREEE
  : valid_(false),
    regex_mode_(REG_EXTENDED)
#endif
{
  *this = other;
}

Regex& Regex::operator=(const Regex& other) {
  if (this == &other) {
    return *this;
  }

  regex_str_ = other.regex_str_;
  regex_mode_ = other.regex_mode_;
#ifdef NO_MORE_REGFREEE
  compiled_regex_ = other.compiled_regex_;
#else
  if (valid_) {
    regfree(&compiled_regex_);
    regfree(&compiled_full_input_regex_);
  }
  compileRegex(compiled_regex_, regex_str_);
  compileRegex(compiled_full_input_regex_, '^' + regex_str_ + '$');
#endif
  valid_ = other.valid_;
  return *this;
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

  regex_str_ = std::move(other.regex_str_);
  regex_mode_ = other.regex_mode_;
#ifdef NO_MORE_REGFREEE
  compiled_regex_ = std::move(other.compiled_regex_);
#else
  if (valid_) {
    regfree(&compiled_regex_);
    regfree(&compiled_full_input_regex_);
  }
  compiled_regex_ = other.compiled_regex_;
  compiled_full_input_regex_ = other.compiled_full_input_regex_;
#endif
  valid_ = other.valid_;
  other.valid_ = false;
  return *this;
}

#ifndef NO_MORE_REGFREEE
Regex::~Regex() {
  if (valid_) {
    regfree(&compiled_regex_);
    regfree(&compiled_full_input_regex_);
  }
}
#endif

#ifndef NO_MORE_REGFREEE
void Regex::compileRegex(regex_t& regex, const std::string& regex_string) const {
  int err_code = regcomp(&regex, regex_string.c_str(), regex_mode_);
  if (err_code) {
    const size_t size = regerror(err_code, &regex, nullptr, 0);
    std::vector<char> msg(size);
    regerror(err_code, &regex, msg.data(), msg.size());
    throw Exception(REGEX_EXCEPTION, std::string(msg.begin(), msg.end()));
  }
}
#endif

bool regexSearch(const std::string &string, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(string, regex.compiled_regex_);
#else
  std::vector<regmatch_t> match;
  match.resize(getMaxGroupCountOfRegex(regex.regex_str_));
  return regexec(&regex.compiled_regex_, string.c_str(), match.size(), match.data(), 0) == 0;
#endif
}

bool regexSearch(const std::string &string, SMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(string, match, regex.compiled_regex_);
#else
  match.clear();
  std::vector<regmatch_t> regmatches;
  regmatches.resize(getMaxGroupCountOfRegex(regex.regex_str_));
  bool result = regexec(&regex.compiled_regex_, string.c_str(), regmatches.size(), regmatches.data(), 0) == 0;
  match.string_ = string;
  for (const auto& regmatch : regmatches) {
    match.matches_.push_back(SMatch::Regmatch{regmatch, match.string_});
  }
  return result;
#endif
}

bool regexMatch(const std::string &string, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(string, regex.compiled_regex_);
#else
  std::vector<regmatch_t> match;
  match.resize(getMaxGroupCountOfRegex(regex.regex_str_));
  return regexec(&regex.compiled_full_input_regex_, string.c_str(), match.size(), match.data(), 0) == 0;
#endif
}

bool regexMatch(const std::string &string, SMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(string, match, regex.compiled_regex_);
#else
  match.clear();
  std::vector<regmatch_t> regmatches;
  regmatches.resize(getMaxGroupCountOfRegex(regex.regex_str_));
  bool result = regexec(&regex.compiled_full_input_regex_, string.c_str(), regmatches.size(), regmatches.data(), 0) == 0;
  match.string_ = string;
  for (const auto& regmatch : regmatches) {
    match.matches_.push_back(SMatch::Regmatch{regmatch, match.string_});
  }
  return result;
#endif
}

SMatch getLastRegexMatch(const std::string& string, const utils::Regex& regex) {
#ifdef NO_MORE_REGFREEE
  auto matches = std::sregex_iterator(string.begin(), string.end(), regex.compiled_regex_);
  std::smatch last_match;
  while (matches != std::sregex_iterator()) {
    last_match = *matches;
    matches = std::next(matches);
  }
  return last_match;
#else
  SMatch search_result;
  SMatch last_match;
  auto current_str = string;
  while (regexSearch(current_str, search_result, regex)) {
    last_match = search_result;
    current_str = search_result.suffix();
  }

  auto diff = string.size() - last_match.string_.size();
  last_match.string_ = string;
  for (auto& match : last_match.matches_) {
    if (match.match.rm_so >= 0) {
      match.match.rm_so += diff;
      match.match.rm_eo += diff;
    }
  }
  return last_match;
#endif
}

}  // namespace org::apache::nifi::minifi::utils
