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
#include <regex>
#define NO_MORE_REGFREEE
#endif

namespace org::apache::nifi::minifi::utils {

class Regex;

#ifdef NO_MORE_REGFREEE
using SMatch = std::smatch;
#else
class SMatch {
  struct Regmatch;
  struct SuffixWrapper;
 public:
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = Regmatch;
    using pointer           = value_type*;
    using reference         = value_type&;

    Iterator() : regmatch_(nullptr) {
    }

    explicit Iterator(Regmatch* regmatch)
      : regmatch_(regmatch) {
    }

    reference operator*() const { return *regmatch_; }
    pointer operator->() { return regmatch_; }

    Iterator& operator++() { regmatch_++; return *this; }
    Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

    friend bool operator== (const Iterator& a, const Iterator& b) { return a.regmatch_ == b.regmatch_; }
    friend bool operator!= (const Iterator& a, const Iterator& b) { return a.regmatch_ != b.regmatch_; }

   private:
    pointer regmatch_;
  };

  SuffixWrapper suffix() const;
  const Regmatch& operator[](std::size_t index) const;
  Iterator begin() { return Iterator(&matches_[0]); }
  Iterator end() { return Iterator(&matches_[matches_.size()]); }

  std::size_t size() const;
  bool ready() const;
  std::size_t position(std::size_t index) const;
  std::size_t length(std::size_t index) const;

 private:
  struct Regmatch {
    operator std::string() const {
      return str();
    }

    std::string str() const {
      if (match.rm_so == -1) {
        return "";
      }
      return std::string(string.begin() + match.rm_so, string.begin() + match.rm_eo);
    }

    regmatch_t match;
    std::string_view string;
  };

  struct SuffixWrapper {
    operator std::string() const {
      return str();
    }

    std::string str() const {
      return suffix;
    }

    std::string suffix;
  };

  void clear();

  std::vector<Regmatch> matches_;
  std::string string_;

  friend bool regexMatch(const std::string& string, SMatch& match, const Regex& regex);
  friend bool regexSearch(const std::string& string, SMatch& match, const Regex& regex);
  friend utils::SMatch getLastRegexMatch(const std::string& string, const utils::Regex& pattern);
};
#endif

class Regex {
 public:
  enum class Mode { ICASE };

  Regex();
  explicit Regex(std::string value);
  Regex(std::string value, const std::vector<Mode> &mode);
  Regex(const Regex &);
  Regex& operator=(const Regex &);
  Regex(Regex&& other);
  Regex& operator=(Regex&& other);
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

  friend bool regexMatch(const std::string &string, const Regex& regex);
  friend bool regexMatch(const std::string &string, SMatch& match, const Regex& regex);
  friend bool regexSearch(const std::string &string, const Regex& regex);
  friend bool regexSearch(const std::string &string, SMatch& match, const Regex& regex);
  friend SMatch getLastRegexMatch(const std::string& string, const utils::Regex& regex);
};

bool regexMatch(const std::string &string, const Regex& regex);
bool regexMatch(const std::string &string, SMatch& match, const Regex& regex);

bool regexSearch(const std::string &string, const Regex& regex);
bool regexSearch(const std::string &string, SMatch& match, const Regex& regex);

/**
 * Returns the last match of a regular expression within the given string
 * @param string incoming string
 * @param regex the regex to be matched
 * @return the last valid SMatch or a default constructed SMatch (ready() != true) if no matches have been found
 */
SMatch getLastRegexMatch(const std::string& string, const utils::Regex& regex);

}  // namespace org::apache::nifi::minifi::utils
