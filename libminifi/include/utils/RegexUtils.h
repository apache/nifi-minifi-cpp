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

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstddef>

// There is a bug in std::regex implementation of libstdc++ which causes stack overflow on long matches: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86164
// Due to this bug we should use regex.h for regex searches if libstdc++ is used until a fix is released.
#if defined(__GLIBCXX__) || defined(__GLIBCPP__)
#include <regex.h>
#else
#include <regex.h>
#define NO_MORE_REGFREEE
#endif

namespace org::apache::nifi::minifi::utils {

class Regex;

#ifdef NO_MORE_REGFREEE
template<typename It>
using MatchResults = std::match_results<It>;
using SMatch = std::smatch;
#else
class SMatch;

template<typename It>
using MatchResults = SMatch;

class SMatch {
  struct Regmatch;
 public:
  SMatch() = default;
  SMatch(const SMatch&);
  SMatch(SMatch&&);
  SMatch& operator=(const SMatch&);
  SMatch& operator=(SMatch&&);

  const Regmatch& suffix() const;
  const Regmatch& operator[](std::size_t index) const;
  auto begin() { return matches_.begin(); }
  auto end() { return matches_.end(); }

  std::size_t size() const;
  bool empty() const;
  bool ready() const;
  std::size_t position(std::size_t index) const;
  std::size_t length(std::size_t index) const;

 private:
  struct Regmatch : std::pair<std::string::const_iterator, std::string::const_iterator> {
    Regmatch(bool matched, std::string::const_iterator begin, std::string::const_iterator end): pair{begin, end}, matched(matched) {}

    operator std::string() const {
      return str();
    }

    std::string str() const {
      if (!matched) {
        return "";
      }
      return std::string(first, second);
    }

    std::size_t length() const {
      return std::distance(first, second);
    }

    bool matched;
  };

  void reset(std::string str);

  bool ready_{false};
  std::vector<Regmatch> matches_;
  std::string string_;
  Regmatch unmatched_{false, string_.end(), string_.end()};
  Regmatch suffix_{unmatched_};

  template<typename T>
  friend bool regexMatch(const T& str, MatchResults<decltype(std::cbegin(str))>& match, const Regex& regex);
  template<typename T>
  friend bool regexSearch(const T& str, MatchResults<decltype(std::cbegin(str))>& match, const Regex& regex);

  friend utils::SMatch getLastRegexMatch(const std::string& string, const utils::Regex& pattern);
};
#endif

using SVMatch = MatchResults<std::string_view::const_iterator>;
using CMatch = MatchResults<const char*>;

class Regex {
 public:
  enum class Mode { ICASE };

  Regex();
  explicit Regex(std::string value);
  Regex(std::string value, const std::vector<Mode> &mode);
  Regex(const Regex &);
  Regex& operator=(const Regex &);
  Regex(Regex&& other) noexcept;
  Regex& operator=(Regex&& other) noexcept;
#ifndef NO_MORE_REGFREEE
  ~Regex();
#endif

 private:
  std::string regex_str_;
  bool valid_;

#ifdef NO_MORE_REGFREEE
  std::regex compiled_regex_;
  std::regex_constants::syntax_option_type regex_mode_;
#else
  void compileRegex(regex_t& regex, const std::string& regex_string) const;

  regex_t compiled_regex_;
  regex_t compiled_full_input_regex_;
  int regex_mode_;
#endif

  template<typename T>
  friend bool regexMatch(const T& str, MatchResults<decltype(std::cbegin(str))>& match, const Regex& regex);
  template<typename T>
  friend bool regexSearch(const T& str, MatchResults<decltype(std::cbegin(str))>& match, const Regex& regex);

  friend SMatch getLastRegexMatch(const std::string& string, const utils::Regex& regex);
};

template<typename T>
bool regexMatch(const T& str, const Regex& regex) {
  MatchResults<decltype(std::cbegin(str))> match;
  return regexMatch(str, match, regex);
}

template<typename T>
bool regexMatch(const T& str, MatchResults<decltype(std::cbegin(str))>& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_match(std::cbegin(str), std::cend(str), match, regex.compiled_regex_);
#else
  return regexMatch(std::string{str}, match, regex);
#endif
}

template<>
bool regexMatch<std::string>(const std::string& str, SMatch& match, const Regex& regex) {
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
        match.matches_.push_back(SMatch::Regmatch{true, begin, end});
      } else {
        match.matches_.push_back(SMatch::Regmatch{false, match.string_.end(), match.string_.end()});
      }
    }
  }
  return result;
#endif
}

template<typename T>
bool regexSearch(const T& str, const Regex& regex) {
  MatchResults<decltype(std::cbegin(str))> match;
  return regexSearch(str, match, regex);
}

template<typename T>
bool regexSearch(const T& str, MatchResults<decltype(std::cbegin(str))>& match, const Regex& regex) {
  if (!regex.valid_) {
    return false;
  }
#ifdef NO_MORE_REGFREEE
  return std::regex_search(std::cbegin(str), std::cend(str), match, regex.compiled_regex_);
#else
  return regexSearch(std::string{str}, match, regex);
#endif
}

template<>
bool regexSearch<std::string>(const std::string& str, SMatch& match, const Regex& regex) {
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
        match.matches_.push_back(SMatch::Regmatch{true, begin, end});
      } else {
        match.matches_.push_back(SMatch::Regmatch{false, match.string_.end(), match.string_.end()});
      }
    }
  }
  return result;
#endif
}

/**
 * Returns the last match of a regular expression within the given string
 * @param string incoming string
 * @param regex the regex to be matched
 * @return the last valid SMatch or a default constructed SMatch (ready() != true) if no matches have been found
 */
SMatch getLastRegexMatch(const std::string& string, const utils::Regex& regex);

}  // namespace org::apache::nifi::minifi::utils
