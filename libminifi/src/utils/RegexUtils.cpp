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

namespace org::apache::nifi::minifi::utils {

#ifndef NO_MORE_REGFREEE

SMatch::SMatch(const SMatch& other) {
  *this = other;
}

SMatch::SMatch(SMatch&& other) {
  *this = std::move(other);
}

SMatch& SMatch::operator=(const SMatch& other) {
  if (this == &other) {
    return *this;
  }
  reset(other.string_);
  matches_.reserve(other.matches_.size());
  ready_ = other.ready_;
  for (const auto& sub_match : other.matches_) {
    size_t begin_off = gsl::narrow<size_t>(std::distance(other.string_.begin(), sub_match.first));
    size_t end_off = gsl::narrow<size_t>(std::distance(other.string_.begin(), sub_match.second));
    matches_.push_back(Regmatch{sub_match.matched, string_.begin() + begin_off, string_.begin() + end_off});
  }
  return *this;
}

SMatch& SMatch::operator=(SMatch&& other) {
  // trigger the copy assignment, we could optimize this (by moving the string/matches)
  // but we would need to maintain a separate offsets vector, as after the move the original
  // sub_matches' iterators are invalidated, if this turns out to be a performance bottleneck
  // revisit this
  return *this = other;
}

const SMatch::Regmatch& SMatch::suffix() const {
  return suffix_;
}

const SMatch::Regmatch& SMatch::operator[](std::size_t index) const {
  if (index >= matches_.size()) {
    return unmatched_;
  }
  return matches_[index];
}

std::size_t SMatch::size() const {
  return matches_.size();
}

bool SMatch::empty() const {
  return size() == 0;
}

bool SMatch::ready() const {
  return ready_;
}

std::size_t SMatch::position(std::size_t index) const {
  if (index >= matches_.size()) {
    return std::distance(string_.begin(), unmatched_.first);
  }
  return std::distance(string_.begin(), matches_[index].first);
}

std::size_t SMatch::length(std::size_t index) const {
  if (index >= matches_.size()) {
    return std::distance(unmatched_.first, unmatched_.second);
  }
  return std::distance(matches_[index].first, matches_[index].second);
}

void SMatch::reset(std::string str) {
  matches_.clear();
  string_ = std::move(str);
  unmatched_ = Regmatch{false, string_.end(), string_.end()};
  suffix_ = unmatched_;
  ready_ = false;
}
#endif

Regex::Regex() : Regex::Regex("") {}

Regex::Regex(std::string value) : Regex::Regex(std::move(value), {}) {}

Regex::Regex(std::string value, const std::vector<Regex::Mode> &mode)
  : regex_str_(std::move(value)),
    valid_(false),
#ifdef NO_MORE_REGFREEE
    regex_mode_(std::regex_constants::ECMAScript) {
#else
    regex_mode_(REG_EXTENDED) {
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
  : valid_(false),
#ifndef NO_MORE_REGFREEE
    regex_mode_(REG_EXTENDED)
#else
    regex_mode_(std::regex_constants::ECMAScript)
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

Regex::Regex(Regex&& other) noexcept
  : valid_(false),
#ifndef NO_MORE_REGFREEE
    regex_mode_(REG_EXTENDED)
#else
    regex_mode_(std::regex_constants::ECMAScript)
#endif
{
  *this = std::move(other);
}

Regex& Regex::operator=(Regex&& other) noexcept {
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

bool regexMatch(const char* str, const Regex& regex) {
  CMatch match;
  return regexMatch(str, match, regex);
}

bool regexMatch(const std::string_view& str, const Regex& regex) {
  SVMatch match;
  return regexMatch(str, match, regex);
}

bool regexMatch(const std::string& str, const Regex& regex) {
  SMatch match;
  return regexMatch(str, match, regex);
}

bool regexMatch(const char* str, CMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(str, str + std::strlen(str), match, regex.compiled_regex_);
#else
  return regexMatch(std::string{str}, match, regex);
#endif
}

bool regexMatch(const std::string_view& str, SVMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(std::cbegin(str), std::cend(str), match, regex.compiled_regex_);
#else
  return regexMatch(std::string{str}, match, regex);
#endif
}

bool regexMatch(const std::string& str, SMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(str, match, regex.compiled_regex_);
#else
  match.reset(str);
  match.ready_ = true;
  std::vector<regmatch_t> regmatches(regex.compiled_full_input_regex_.re_nsub + 1);
  bool result = regexec(&regex.compiled_full_input_regex_, str.c_str(), regmatches.size(), regmatches.data(), 0) == 0;
  if (result) {
    match.suffix_ = SMatch::Regmatch{true, match.string_.begin() + regmatches[0].rm_eo, match.string_.end()};
    for (const auto& regmatch : regmatches) {
      bool matched = regmatch.rm_so != -1;
      if (matched) {
        auto begin = match.string_.begin() + regmatch.rm_so;
        auto end = match.string_.begin() + regmatch.rm_eo;
        match.matches_.emplace_back(true, begin, end);
      } else {
        match.matches_.emplace_back(false, match.string_.end(), match.string_.end());
      }
    }
  }
  return result;
#endif
}

bool regexSearch(const char* str, const Regex& regex) {
  CMatch match;
  return regexSearch(str, match, regex);
}

bool regexSearch(const std::string_view& str, const Regex& regex) {
  SVMatch match;
  return regexSearch(str, match, regex);
}

bool regexSearch(const std::string& str, const Regex& regex) {
  SMatch match;
  return regexSearch(str, match, regex);
}

bool regexSearch(const char* str, CMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(str, str + std::strlen(str), match, regex.compiled_regex_);
#else
  return regexSearch(std::string{str}, match, regex);
#endif
}

bool regexSearch(const std::string_view& str, SVMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(std::cbegin(str), std::cend(str), match, regex.compiled_regex_);
#else
  return regexSearch(std::string{str}, match, regex);
#endif
}

bool regexSearch(const std::string& str, SMatch& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(str, match, regex.compiled_regex_);
#else
  match.reset(str);
  match.ready_ = true;
  std::vector<regmatch_t> regmatches(regex.compiled_regex_.re_nsub + 1);
  bool result = regexec(&regex.compiled_regex_, str.c_str(), regmatches.size(), regmatches.data(), 0) == 0;
  if (result) {
    match.suffix_ = SMatch::Regmatch{true, match.string_.begin() + regmatches[0].rm_eo, match.string_.end()};
    for (const auto& regmatch : regmatches) {
      bool matched = regmatch.rm_so != -1;
      if (matched) {
        auto begin = match.string_.begin() + regmatch.rm_so;
        auto end = match.string_.begin() + regmatch.rm_eo;
        match.matches_.emplace_back(true, begin, end);
      } else {
        match.matches_.emplace_back(false, match.string_.end(), match.string_.end());
      }
    }
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

  if (!last_match.ready()) {
    return last_match;
  }

  struct MatchInfo {
    bool matched;
    size_t begin;
    size_t end;
  };

  // we must save the sub matches' info in a way that does
  // not get invalidated by SMatch::reset, and can be transferred
  // to the updated match
  std::vector<MatchInfo> match_infos;
  match_infos.reserve(last_match.size());
  for (auto& match : last_match.matches_) {
    match_infos.emplace_back(MatchInfo{
      .matched = match.matched,
      .begin = gsl::narrow<size_t>(std::distance(last_match.string_.cbegin(), match.first)),
      .end = gsl::narrow<size_t>(std::distance(last_match.string_.cbegin(), match.second))
    });
  }
  // offset of the start of the last match into the original string
  auto offset = string.size() - last_match.string_.size();
  last_match.reset(string);
  last_match.ready_ = true;
  for (auto& info : match_infos) {
    size_t match_off = info.matched ? offset : 0;
    last_match.matches_.emplace_back(SMatch::Regmatch{
      info.matched,
      last_match.string_.cbegin() + info.begin + match_off,
      last_match.string_.cbegin() + info.end + match_off
    });
  }
  return last_match;
#endif
}

}  // namespace org::apache::nifi::minifi::utils
