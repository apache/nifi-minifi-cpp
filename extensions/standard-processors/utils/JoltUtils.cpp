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

#include "JoltUtils.h"
#include "rapidjson/error/en.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::utils::jolt {


static bool isSpecialChar(char ch) {
  static constexpr std::array SPECIAL_CHARS{'.', '[', ']', '$', '&', '@', '#', '*'};
  return std::find(SPECIAL_CHARS.begin(), SPECIAL_CHARS.end(), ch) != SPECIAL_CHARS.end();
}

bool Spec::Template::check(std::string_view str) {
  enum class State {
    Plain,
    Escaped
  } state = State::Plain;
  for (char ch : str) {
    switch (state) {
      case State::Plain: {
        if (ch == '&') {
          return true;
        } else if (ch == '\\') {
          state = State::Escaped;
        }
        break;
      }
      case State::Escaped: {
        state = State::Plain;
        break;
      }
    }
  }
  return false;
}

nonstd::expected<std::pair<Spec::Template, Spec::It>, std::string> Spec::Template::parse(It begin, It end) {
  enum class State {
    Plain,
    Escaped,
    Template,  // &
    SimpleIndex,  // &1
    CanonicalTemplate,  // &(
    ParentIndex,  // &(1
    NextIndex,  // &(1,
    MatchIndex  // &(1,0
  };

  std::vector<std::string> fragments;
  std::vector<std::pair<size_t, size_t>> references;
  fragments.push_back({});
  State state = State::Plain;
  std::string target;
  // go beyond the last char on purpose
  auto ch_it = begin;
  while (ch_it <= end) {
    std::optional<char> ch;
    if (ch_it < end) {
      ch = *ch_it;
    }
    bool force_terminate = false;
    switch (state) {
      case State::Plain: {
        if (ch == '\\') {
          state = State::Escaped;
        } else if (ch == '&') {
          references.push_back({});
          fragments.push_back({});
          state = State::Template;
        } else if (ch == ')' || ch == ']' || ch == '.' || ch == '[') {
          force_terminate = true;
        } else if (ch) {
          fragments.back() += ch.value();
        }
        break;
      }
      case State::Escaped: {
        if (!ch) {
          return nonstd::make_unexpected("Unterminated escape sequence");
        }
        if (ch != '\\' && !isSpecialChar(ch.value())) {
          return nonstd::make_unexpected(fmt::format("Unknown escape sequence in template '\\{}'", ch.value()));
        }
        fragments.back() += ch.value();
        state = State::Plain;
        break;
      }
      case State::Template: {
        if (ch == '(') {
          state = State::CanonicalTemplate;
        } else if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::SimpleIndex;
        } else {
          state = State::Plain;
          // reprocess this char in a different state
          gsl_Expects(ch_it != begin);
          --ch_it;
        }
        break;
      }
      case State::SimpleIndex: {
        if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target += ch.value();
        } else {
          references.back().first = std::stoi(target);
          state = State::Plain;
          // reprocess this char in a different state
          gsl_Expects(ch_it != begin);
          --ch_it;
        }
        break;
      }
      case State::CanonicalTemplate: {
        if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::ParentIndex;
        } else {
          return nonstd::make_unexpected(fmt::format("Expected an index at {}", std::distance(begin, ch_it)));
        }
        break;
      }
      case State::ParentIndex: {
        if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target += ch.value();
        } else if (ch == ',') {
          references.back().first = std::stoi(target);
          state = State::NextIndex;
        } else if (ch == ')') {
          references.back().first = std::stoi(target);
          state = State::Plain;
        } else {
          return nonstd::make_unexpected(fmt::format("Invalid character at {}, expected digit, comma or close parenthesis", std::distance(begin, ch_it)));
        }
        break;
      }
      case State::NextIndex: {
        if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::MatchIndex;
        } else {
          return nonstd::make_unexpected(fmt::format("Expected an index at {}", std::distance(begin, ch_it)));
        }
        break;
      }
      case State::MatchIndex: {
        if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target += ch.value();
        } else if (ch == ')') {
          references.back().second = std::stoi(target);
          state = State::Plain;
        } else {
          return nonstd::make_unexpected(fmt::format("Invalid character at {}, expected digit or close parenthesis", std::distance(begin, ch_it)));
        }
        break;
      }
    }
    if (force_terminate) {
      break;
    }
    if (ch_it != end) {
      ++ch_it;
    } else {
      break;
    }
  }

  gsl_Assert(state == State::Plain);
  return std::pair<Template, It>{Template{std::move(fragments), std::move(references)}, ch_it};
}

bool Spec::Regex::check(std::string_view str) {
  enum class State {
    Plain,
    Escaped
  } state = State::Plain;
  for (char ch : str) {
    switch (state) {
      case State::Plain: {
        if (ch == '*') {
          return true;
        } else if (ch == '\\') {
          state = State::Escaped;
        }
        break;
      }
      case State::Escaped: {
        state = State::Plain;
        break;
      }
    }
  }
  return false;
}

nonstd::expected<Spec::Regex, std::string> Spec::Regex::parse(std::string_view str) {
  enum class State {
    Plain,
    Escaped
  };
  std::vector<std::string> fragments;
  fragments.push_back({});
  State state = State::Plain;
  for (size_t idx = 0; idx <= str.size(); ++idx) {
    std::optional<char> ch;
    if (idx < str.size()) {
      ch = str[idx];
    }
    switch (state) {
      case State::Plain: {
        if (ch == '\\') {
          state = State::Escaped;
        } else if (ch == '*') {
          fragments.push_back({});
        } else if (ch) {
          fragments.back() += ch.value();
        }
        break;
      }
      case State::Escaped: {
        if (!ch) {
          return nonstd::make_unexpected("Unterminated escape sequence");
        }
        if (ch != '\\' && !isSpecialChar(ch.value())) {
          return nonstd::make_unexpected(fmt::format("Unknown escape sequence in pattern '\\{}'", ch.value()));
        }
        fragments.back() += ch.value();
        state = State::Plain;
        break;
      }
    }
  }
  gsl_Assert(state == State::Plain);
  return Regex{std::move(fragments)};
}

std::string Spec::Template::eval(const Context& ctx) const {
  std::string res;
  for (size_t idx = 0; idx + 1 < fragments.size(); ++idx) {
    res += fragments.at(idx);
    auto& ref = references.at(idx);
    auto* target = ctx.find(ref.first);
    if (!target) {
      throw Exception(GENERAL_EXCEPTION, fmt::format("Invalid reference to {} at {}", ref.first, ctx.path()));
    }
    if (target->matches.size() <= ref.second) {
      throw Exception(GENERAL_EXCEPTION, fmt::format("Could not find match {} in '{}' at {}", ref.second, target->matches.at(0), ctx.path()));
    }
    res += target->matches.at(ref.second);
  }
  res += fragments.back();
  return res;
}

std::optional<std::vector<std::string_view>> Spec::Regex::match(std::string_view str) const {
  std::vector<std::string_view> matches;
  matches.push_back(str);
  if (fragments.size() == 1) {
    if (str == fragments.front()) {
      return matches;
    } else {
      return std::nullopt;
    }
  }

  // first fragment is at the beginning of the string
  if (!str.starts_with(fragments.front())) {
    return std::nullopt;
  }
  auto it = str.begin() + fragments.front().size();
  for (size_t idx = 1; idx + 1 < fragments.size(); ++idx) {
    auto& frag = fragments[idx];
    auto next_it = std::search(it, str.end(), frag.begin(), frag.end());
    if (next_it == str.end() && !frag.empty()) {
      return std::nullopt;
    }
    matches.push_back({it, next_it});
    it = next_it + frag.size();
  }
  // last fragment is at the end of the string
  if (gsl::narrow<size_t>(std::distance(it, str.end())) < fragments.back().size()) {
    // not enough characters left
    return std::nullopt;
  }
  auto next_it = std::next(str.rbegin(), gsl::narrow<decltype(str.rbegin())::difference_type>(fragments.back().size())).base();
  if (std::string_view(next_it, str.end()) != fragments.back()) {
    return std::nullopt;
  }
  matches.push_back({it, next_it});
  return matches;
}

namespace {

nonstd::expected<std::pair<Spec::Destination, Spec::It>, std::string> parseDestination(const Spec::Context& ctx, Spec::It begin, Spec::It end);
Spec::Destinations parseDestinations(const Spec::Context& ctx, const rapidjson::Value& val);

Spec::Pattern::Value parseValue(const Spec::Context& ctx, const rapidjson::Value& val);

std::pair<size_t, size_t> parseKeyAccess(std::string_view str) {
  enum class State {
    Begin,
    BeginRef,
    PrimaryIndex,
    BeginFirstIndex,
    FirstIndex,
    BeginSecondIndex,
    SecondIndex,
    End
  } state = State::Begin;
  std::string target;
  std::pair<size_t, size_t> result{0, 0};
  for (size_t idx = 0; idx <= str.size(); ++idx) {
    std::optional<char> ch;
    if (idx < str.size()) {
      ch = str[idx];
    }
    switch (state) {
      case State::Begin: {
        if (ch != '$') {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Expected '$' in key access in '{}' at {}", str, idx));
        }
        state = State::BeginRef;
        break;
      }
      case State::BeginRef: {
        if (ch == '(') {
          state = State::BeginFirstIndex;
        } else if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::PrimaryIndex;
        } else if (ch) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Expected index in key access in '{}' at {}", str, idx));
        }
        break;
      }
      case State::PrimaryIndex: {
        if (!ch) {
          result.first = std::stoull(target);
        } else if (std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target += ch.value();
        } else {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Expected digit in key access in '{}' at {}", str, idx));
        }
        break;
      }
      case State::BeginFirstIndex: {
        if (!ch) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Unterminated first index in key access in '{}'", str));
        } else if (std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::FirstIndex;
        } else {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Expected digit in key access in '{}' at {}", str, idx));
        }
        break;
      }
      case State::FirstIndex: {
        if (!ch) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Unterminated first index in key access in '{}'", str));
        } else if (std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target += ch.value();
        } else if (ch == ',') {
          result.first = std::stoull(target);
          state = State::BeginSecondIndex;
        }
        break;
      }
      case State::BeginSecondIndex: {
        if (!ch) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Unterminated second index in key access in '{}'", str));
        } else if (std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::SecondIndex;
        } else {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Expected digit in key access in '{}' at {}", str, idx));
        }
        break;
      }
      case State::SecondIndex: {
        if (!ch) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Unterminated second index in key access in '{}'", str));
        } else if (std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target += ch.value();
        } else if (ch == ')') {
          result.second = std::stoull(target);
          state = State::End;
        }
        break;
      }
      case State::End: {
        if (ch) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Expected end of string in '{}' at {}", str, idx));
        }
        break;
      }
    }
  }
  return result;
}

std::string parseLiteral(std::string_view str) {
  enum class State {
    Plain,
    Escaped
  } state = State::Plain;
  std::string result;
  for (size_t idx = 0; idx <= str.size(); ++idx) {
    std::optional<char> ch;
    if (idx < str.size()) {
      ch = str[idx];
    }
    switch (state) {
      case State::Plain: {
        if (ch == '\\') {
          state = State::Escaped;
        } else if (ch) {
          result += ch.value();
        }
        break;
      }
      case State::Escaped: {
        if (!ch) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Unterminated escape sequence in '{}'", str));
        }
        if (ch != '\\' && !isSpecialChar(ch.value())) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Unknown escape sequence in literal '\\{}'", ch.value()));
        }
        result += ch.value();
        state = State::Plain;
        break;
      }
    }
  }

  gsl_Expects(state == State::Plain);
  return result;
}

nonstd::expected<std::pair<Spec::Path, Spec::It>, std::string> parsePath(const Spec::Context& ctx, Spec::It begin, Spec::It end) {
  auto dst = parseDestination(ctx, begin, end);
  if (!dst) {
    return nonstd::make_unexpected(std::move(dst.error()));
  }
  Spec::Path result;
  for (auto&& [member, type] : std::move(dst->first)) {
    if (!holds_alternative<Spec::Template>(member)) {
      return nonstd::make_unexpected(fmt::format("Value reference at {} cannot contain nested value reference path", ctx.path()));
    }
    result.emplace_back(std::move(std::get<Spec::Template>(member)), type);
  }
  return std::pair<Spec::Path, Spec::It>{result, dst->second};
}

nonstd::expected<std::pair<Spec::ValueRef, Spec::It>, std::string> parseValueReference(const Spec::Context& ctx, Spec::It begin, Spec::It end, bool greedy_path) {
  using ResultT = std::pair<Spec::ValueRef, Spec::It>;
  auto it = begin;
  if (it == end) {
    return nonstd::make_unexpected("Cannot parse value reference from empty string");
  }
  if (*it != '@') {
    return nonstd::make_unexpected("Value reference must start with '@'");
  }
  ++it;
  if (it == end) {
    return ResultT{{0, {}}, it};
  }
  if (*it != '(') {
    if (std::isdigit(static_cast<unsigned char>(*it))) {
      // format is @123...
      auto idx_begin = it;
      while (it != end && std::isdigit(static_cast<unsigned char>(*it))) {
        ++it;
      }
      return ResultT{{std::stoull(std::string{idx_begin, it}), {}}, it};
    }
    // format is @field.inner
    if (greedy_path) {
      if (auto path = parsePath(ctx, it, end)) {
        return ResultT{{0, std::move(path->first)}, path->second};
      } else {
        return ResultT {{0, {}}, it};
      }
    } else {
      if (auto templ = Spec::Template::parse(it, end)) {
        return ResultT{{0, Spec::Path{{std::move(templ->first), Spec::MemberType::FIELD}}}, templ->second};
      } else {
        return ResultT {{0, {}}, it};
      }
    }
  }
  ++it;
  size_t idx = 0;
  if (it != end && std::isdigit(static_cast<unsigned char>(*it))) {
    auto idx_begin = it;
    while (it != end && std::isdigit(static_cast<unsigned char>(*it))) {
      ++it;
    }
    auto idx_end = it;
    idx = std::stoull(std::string{idx_begin, idx_end});
    if (it == end) {
      return nonstd::make_unexpected("Expected ')' in value reference");
    }
    if (*it != ',') {
      if (*it != ')') {
        return nonstd::make_unexpected("Expected ')' in value reference");
      }
      ++it;
      return ResultT{{idx, {}}, it};
    }
    // *it == ','
    ++it;
  }
  if (it == end) {
    return nonstd::make_unexpected("Expected member accessor in value reference");
  }
  auto path = parsePath(ctx, it, end);
  if (!path) {
    return nonstd::make_unexpected(fmt::format("Invalid path in value reference: {}", path.error()));
  }
  it = path->second;
  if (it == end || *it != ')') {
    return nonstd::make_unexpected("Expected ')' in value reference");
  }
  ++it;
  return ResultT{{idx, std::move(path->first)}, it};
}

template<typename T>
bool isAllDigits(T begin, T end) {
  return std::all_of(begin, end, [] (auto ch) {return std::isdigit(static_cast<unsigned char>(ch));});
}

void parseMember(const Spec::Context& ctx, const std::unique_ptr<Spec::Pattern>& result, std::string_view name, const rapidjson::Value& member) {
  if (name.starts_with("@")) {
    if (auto ref = parseValueReference(ctx, name.begin(), name.end(), true)) {
      if (ref->second != name.end()) {
        throw Exception(GENERAL_EXCEPTION, "Failed to fully parse value reference");
      }
      Spec::Context sub_ctx = ctx.extend(ctx.matches, ctx.node);
      result->values.push_back({Spec::ValueRef{ref->first}, parseValue(sub_ctx, member)});
    } else {
      throw Exception(GENERAL_EXCEPTION, fmt::format("Failed to parse value reference at '{}/{}': {}", ctx.path(), name, ref.error()));
    }
  } else if (name.starts_with("$")) {
    Spec::Context sub_ctx = ctx.extend({name}, nullptr);
    result->keys.insert({parseKeyAccess(name), parseDestinations(sub_ctx, member)});
  } else if (name.starts_with("#")) {
    result->defaults.insert({std::string{name.substr(1)}, parseDestinations(ctx, member)});
  } else {
    const bool is_template = Spec::Template::check(name);
    const bool is_regex = Spec::Regex::check(name);
    if (is_template && is_regex) {
      throw Exception(GENERAL_EXCEPTION, "Pattern cannot contain both & and *");
    }
    if (is_template) {
      if (auto templ = Spec::Template::parse(name.begin(), name.end())) {
        if (templ->second != name.end()) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Failed to parse template at {}, unexpected char at {}", ctx.path(), std::distance(name.begin(), templ->second)));
        }
        // dry eval so we can check if the references refer to valid substrings
        (void)templ->first.eval(ctx);
        Spec::Context sub_ctx = ctx.extend({name}, nullptr);
        result->templates.insert({templ->first, parseValue(sub_ctx, member)});
      } else {
        throw Exception(GENERAL_EXCEPTION, fmt::format("Error while parsing key template at {}: {}", ctx.path(), templ.error()));
      }
    } else if (is_regex) {
      if (auto reg = Spec::Regex::parse(name)) {
        Spec::Context sub_ctx = ctx.extend({name}, nullptr);
        sub_ctx.matches.resize(reg.value().size());
        result->regexes.insert({reg.value(), parseValue(sub_ctx, member)});
      } else {
        throw Exception(GENERAL_EXCEPTION, fmt::format("Error while parsing key regex at {}: {}", ctx.path(), reg.error()));
      }
    } else {
      Spec::Context sub_ctx = ctx.extend({name}, nullptr);
      std::optional<size_t> numeric_value;
      auto literal_name = parseLiteral(name);
      result->literal_indices.insert({literal_name, result->literals.size()});
      if (isAllDigits(literal_name.begin(), literal_name.end())) {
        numeric_value = std::stoull(literal_name);
      }
      result->literals.push_back({literal_name, numeric_value, parseValue(sub_ctx, member)});
    }
  }
}

std::unique_ptr<Spec::Pattern> parseMap(const Spec::Context& ctx, const rapidjson::Value& val) {
  if (!val.IsObject()) {
    throw Exception(GENERAL_EXCEPTION, fmt::format("Expected a map at '{}'", ctx.path()));
  }
  auto map = std::make_unique<Spec::Pattern>();

  enum class State {
    Plain,
    Escaped
  } state = State::Plain;

  for (auto& [name_val, member] : val.GetObject()) {
    std::string_view name{name_val.GetString(), name_val.GetStringLength()};
    std::string subkey;
    for (size_t idx = 0; idx <= name.size(); ++idx) {
      std::optional<char> ch;
      if (idx < name.size()) {
        ch = name[idx];
      }
      switch (state) {
        case State::Plain: {
          if (ch == '\\') {
            state = State::Escaped;
          } else if (!ch || ch == '|') {
            parseMember(ctx, map, subkey, member);
            subkey.clear();
          } else {
            subkey += ch.value();
          }
          break;
        }
        case State::Escaped: {
          if (!ch) {
            throw Exception(GENERAL_EXCEPTION, "Unterminated escape sequence");
          }
          if (ch == '|') {
            // this is an extension so we can escape '|' characters
            subkey += "|";
          } else {
            // leave the escape character for further processing
            subkey += "\\";
            subkey += ch.value();
          }
          state = State::Plain;
          break;
        }
      }
    }
  }
  return map;
}

nonstd::expected<std::pair<Spec::MatchingIndex, Spec::It>, std::string> parseMatchingIndex(Spec::It begin, Spec::It end) {
  auto it = begin;
  if (it == end) {
    return nonstd::make_unexpected("Empty matching index");
  }
  if (*it != '#') {
    return nonstd::make_unexpected("Matching must start with a '#'");
  }
  ++it;
  auto idx_begin = it;
  while (it != end && std::isdigit(static_cast<unsigned char>(*it))) {
    ++it;
  }
  return std::pair<Spec::MatchingIndex, Spec::It>{std::stoull(std::string{idx_begin, it}), it};
}

// dot-delimited list of templates and value references
nonstd::expected<std::pair<Spec::Destination, Spec::It>, std::string> parseDestination(const Spec::Context& ctx, Spec::It begin, Spec::It end) {
  Spec::Destination result;
  Spec::MemberType type = Spec::MemberType::FIELD;
  auto ch_it = begin;
  auto isEnd = [&] () {
    return ch_it == end || *ch_it == ')';
  };
  while (!isEnd()) {
    if (auto match_idx = parseMatchingIndex(ch_it, end)) {
      if (type != Spec::MemberType::INDEX) {
        return nonstd::make_unexpected("Matching index can only be used in index context, e.g. apple[#2]");
      }
      if (!ctx.find(match_idx->first)) {
        return nonstd::make_unexpected(fmt::format("Invalid matching index at {} to ancestor {}", ctx.path(), match_idx->first));
      }
      Spec::Destination::value_type result_element{match_idx->first, type};
      result.push_back(result_element);
      ch_it = match_idx->second;
    } else if (auto val_ref = parseValueReference(ctx, ch_it, end, false)) {
      result.push_back({std::move(val_ref->first), type});
      ch_it = val_ref->second;
    } else if (auto templ = Spec::Template::parse(ch_it, end)) {
      // dry eval to verify that references are valid
      (void)templ->first.eval(ctx);
      result.push_back({std::move(templ->first), type});
      ch_it = templ->second;
    } else {
      return nonstd::make_unexpected(fmt::format("Could not parse neither value reference or template in {} at {}", ctx.path(), std::distance(begin, ch_it)));
    }
    if (type == Spec::MemberType::INDEX) {
      if (ch_it == end || *ch_it != ']') {
        return nonstd::make_unexpected(fmt::format("Expected closing index ']' in {} at {}", ctx.path(), std::distance(begin, ch_it)));
      }
      ++ch_it;
    }
    if (!isEnd()) {
      if (*ch_it == '.') {
        type = Spec::MemberType::FIELD;
      } else if (*ch_it == '[') {
        type = Spec::MemberType::INDEX;
      } else {
        return nonstd::make_unexpected(fmt::format("Unexpected destination delimiter '{}' in {} at {}", *ch_it, ctx.path(), std::distance(begin, ch_it)));
      }
      ++ch_it;
      if (ch_it == end) {
        if (type == Spec::MemberType::FIELD) {
          return nonstd::make_unexpected(fmt::format("Unterminated member in {} at {}", ctx.path(), std::distance(begin, ch_it)));
        } else {
          return nonstd::make_unexpected(fmt::format("Unterminated indexed member in {} at {}", ctx.path(), std::distance(begin, ch_it)));
        }
      }
    }
  }

  return std::pair<Spec::Destination, Spec::It>{result, ch_it};
}

Spec::Destinations parseDestinations(const Spec::Context& ctx, const rapidjson::Value& val) {
  Spec::Destinations res;
  if (val.IsNull()) {
    return res;
  }
  if (val.IsArray()) {
    for (rapidjson::SizeType i = 0; i < val.GetArray().Size(); ++i) {
      auto& item = val.GetArray()[i];
      if (!item.IsString()) {
        throw Exception(GENERAL_EXCEPTION, fmt::format("Expected a string or array of strings at '{}/{}'", ctx.path(), i));
      }
      std::string_view item_str{item.GetString(), item.GetStringLength()};
      if (auto dst = parseDestination(ctx, item_str.begin(), item_str.end())) {
        if (dst->second != item_str.end()) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Failed to fully parse destination at '{}/{}'", ctx.path(), i));
        }
        res.push_back(std::move(dst->first));
      } else {
        throw Exception(GENERAL_EXCEPTION, fmt::format("Failed to parse destination at '{}/{}': {}", ctx.path(), i, dst.error()));
      }
    }
  } else {
    if (!val.IsString()) {
      throw Exception(GENERAL_EXCEPTION, fmt::format("Expected a string or array of strings at '{}'", ctx.path()));
    }
    std::string_view val_str{val.GetString(), val.GetStringLength()};
    if (auto dst = parseDestination(ctx, val_str.begin(), val_str.end())) {
      if (dst->second != val_str.end()) {
        throw Exception(GENERAL_EXCEPTION, fmt::format("Failed to fully parse destination at '{}'", ctx.path()));
      }
      res.push_back(std::move(dst->first));
    } else {
      throw Exception(GENERAL_EXCEPTION, fmt::format("Failed to parse destination at '{}': {}", ctx.path(), dst.error()));
    }
  }
  return res;
}

std::optional<std::string> jsonValueToString(const rapidjson::Value& val) {
  if (val.IsString()) {
    return std::string{val.GetString(), val.GetStringLength()};
  }
  if (val.IsUint64()) {
    return std::to_string(val.GetUint64());
  }
  if (val.IsInt64()) {
    return std::to_string(val.GetInt64());
  }
  if (val.IsDouble()) {
    return std::to_string(static_cast<int64_t>(val.GetDouble()));
  }
  if (val.IsBool()) {
    return val.GetBool() ? "true" : "false";
  }
  return std::nullopt;
}

std::pair<std::string, std::string> toString(const Spec::Context& ctx, const Spec::Path& path) {
  std::string raw;
  std::string result;
  for (auto& [member, type] : path) {
    if (type == Spec::MemberType::FIELD) {
      raw += "." + member.full;
      result += "." + member.eval(ctx);
    } else {
      raw += "[" + member.full + "]";
      raw += "[" + member.eval(ctx) + "]";
    }
  }
  return {raw, result};
}

nonstd::expected<std::reference_wrapper<const rapidjson::Value>, std::string> resolvePath(const Spec::Context& ctx, std::string_view root_path, const rapidjson::Value& root, const Spec::Path& path);

Spec::Pattern::Value parseValue(const Spec::Context& ctx, const rapidjson::Value& val) {
  if (val.IsObject()) {
    return parseMap(ctx, val);
  }
  return parseDestinations(ctx, val);
}

void putValue(const Spec::Context& ctx, const Spec::Destination& dest, const rapidjson::Value& val, rapidjson::Document& output) {
  std::vector<std::pair<std::string, Spec::MemberType>> evaled_dest;

  for (auto& [templ, type] : dest) {
    if (auto* val_ref = std::get_if<Spec::ValueRef>(&templ)) {
      auto* root = ctx.find(val_ref->first);
      if (!root) {
        throw Exception(GENERAL_EXCEPTION, fmt::format("Could not find ancestor at index {} from {}", val_ref->first, ctx.path()));
      }
      if (!root->node) {
        return;
      }
      std::reference_wrapper<const rapidjson::Value> member_value = std::ref(*root->node);
      if (auto inner_member_value = resolvePath(ctx, ctx.path(), *root->node, val_ref->second)) {
        member_value = inner_member_value.value();
      } else {
        auto sub_path = toString(ctx, val_ref->second);
        ctx.log([&] (const auto& logger) {
          logger->log_trace("Could not find member at @({},{} as {}) from {}", val_ref->first, sub_path.first, sub_path.second, ctx.path());
        }, [] (const auto&) {});
        // do not write anything and do not throw
        return;
      }
      if (type == Spec::MemberType::INDEX) {
        size_t idx{};
        if (member_value.get().IsUint64()) {
          idx = gsl::narrow<size_t>(member_value.get().GetUint64());
        } else if (member_value.get().IsInt64()) {
          int64_t idx_val = member_value.get().GetInt64();
          if (idx_val < 0) {
            return;
          }
          idx = gsl::narrow<size_t>(idx_val);
        } else if (member_value.get().IsDouble()) {
          // no words
          double idx_val = member_value.get().GetDouble();
          if (idx_val < 0) {
            return;
          }
          idx = static_cast<size_t>(idx_val);
        } else if (member_value.get().IsString()) {
          // amazing... not
          if (isAllDigits(member_value.get().GetString(), member_value.get().GetString() + member_value.get().GetStringLength())) {
            idx = std::stoull(std::string{member_value.get().GetString(), member_value.get().GetStringLength()});
          } else {
            return;
          }
        } else {
          return;
        }
        evaled_dest.push_back({std::to_string(idx), type});
      } else {
        auto member = jsonValueToString(member_value);
        if (!member) {
          return;
        }
        evaled_dest.push_back({std::move(member.value()), type});
      }
      continue;
    }

    if (auto* match_idx = std::get_if<Spec::MatchingIndex>(&templ)) {
      gsl_Expects(type == Spec::MemberType::INDEX);
      auto* target = ctx.find(*match_idx);
      if (!target) {
        throw Exception(GENERAL_EXCEPTION, fmt::format("Could not find ancestor at index {} from {}", *match_idx, ctx.path()));
      }
      evaled_dest.push_back({std::to_string(target->match_count), type});
      continue;
    }

    // empty segment is self-reference, e.g. a..b == a.b
    if (type == Spec::MemberType::FIELD && get<Spec::Template>(templ).empty()) {
      continue;
    }

    evaled_dest.push_back({get<Spec::Template>(templ).eval(ctx), type});
  }

  std::reference_wrapper<rapidjson::Value> target = output;
  for (auto& [member, type] : evaled_dest) {
    if (type == Spec::MemberType::INDEX) {
      if (!target.get().IsArray()) {
        if (!target.get().IsNull()) {
          throw Exception(GENERAL_EXCEPTION, "Cannot write based on index into non-array");
        }
        target.get().SetArray();
      }
      size_t idx = [&, member_ptr = &member] () -> size_t {
        if (member_ptr->empty()) {
          // an empty index like "field.inner[]" means to append to the end
          return gsl::narrow<size_t>(target.get().Size());
        }
        try {
          return std::stoull(*member_ptr);
        } catch (const std::exception&) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Could not convert '{}' to index", *member_ptr));
        }
      }();
      target.get().Reserve(gsl::narrow<rapidjson::SizeType>(idx + 1), output.GetAllocator());
      for (size_t arr_idx = target.get().Size(); arr_idx <= idx; ++arr_idx) {
        target.get().PushBack(rapidjson::Value{}, output.GetAllocator());
      }
      target = target.get()[gsl::narrow<rapidjson::SizeType>(idx)];
    } else {
      if (!target.get().IsObject()) {
        if (!target.get().IsNull()) {
          throw Exception(GENERAL_EXCEPTION, "Cannot write member into non-object");
        }
        target.get().SetObject();
      }
      if (!target.get().HasMember(member)) {
        target.get().AddMember(rapidjson::Value{member.c_str(), gsl::narrow<rapidjson::SizeType>(member.size()), output.GetAllocator()}, rapidjson::Value{}, output.GetAllocator());
      }
      target = target.get()[member];
    }
  }

  if (!target.get().IsNull()) {
    if (!target.get().IsArray()) {
      // put it in an array
      rapidjson::Value tmp{target.get().Move(), output.GetAllocator()};
      target.get().SetArray();
      target.get().GetArray().PushBack(tmp.Move(), output.GetAllocator());
    }
    target.get().PushBack(rapidjson::Value{}, output.GetAllocator());
    target = target.get()[target.get().GetArray().Size() - 1];
  }
  target.get().CopyFrom(val, output.GetAllocator());
}

void putValue(const Spec::Context& ctx, const Spec::Destinations& destinations, const rapidjson::Value& val, rapidjson::Document& output) {
  for (auto& dest : destinations) {
    putValue(ctx, dest, val, output);
  }
}

nonstd::expected<std::reference_wrapper<const rapidjson::Value>, std::string> resolvePath(const Spec::Context& ctx, std::string_view root_path, const rapidjson::Value& root, const Spec::Path& path) {
  std::string full_path{root_path};
  std::reference_wrapper<const rapidjson::Value> result = std::ref(root);
  for (auto& [templ, type] : path) {
    auto member = templ.eval(ctx);
    if (type == Spec::MemberType::FIELD) {
      full_path += "." + member;
      if (!result.get().IsObject()) {
        return nonstd::make_unexpected(fmt::format("Expected object at {}", full_path));
      }
      if (!result.get().HasMember(member)) {
        return nonstd::make_unexpected(fmt::format("Object does not have member '{}' at {}", member, full_path));
      }
      result = result.get()[member];
    } else if (type == Spec::MemberType::INDEX) {
      size_t idx = std::stoull(member);
      full_path += "[" + member + "]";
      if (!result.get().IsArray()) {
        return nonstd::make_unexpected(fmt::format("Expected array at {}", full_path));
      }
      if (result.get().Size() <= idx) {
        return nonstd::make_unexpected(fmt::format("Array of size {} does not have item at index {}  at {}", result.get().Size(), idx, full_path));
      }
      result = result.get()[gsl::narrow<rapidjson::SizeType>(idx)];
    }
  }
  return result;
}

}  // namespace

nonstd::expected<Spec, std::string> Spec::parse(std::string_view str, std::shared_ptr<core::logging::Logger> logger) {
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(str.data(), str.length());
  if (!res) {
    return nonstd::make_unexpected(fmt::format("{} at {}", rapidjson::GetParseError_En(res.Code()), res.Offset()));
  }
  try {
    Spec::Context ctx{.matches = {"root"}, .logger = std::move(logger)};
    return Spec{parseMap(ctx, doc)};
  } catch (const std::exception& ex) {
    return nonstd::make_unexpected(ex.what());
  }
}

void Spec::Pattern::process(const Value& val, const Context& ctx, const rapidjson::Value& input, rapidjson::Document& output) {
  std::visit([&] (auto& val) {
    if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::unique_ptr<Pattern>>) {
      val->process(ctx, input, output);
    } else {
      putValue(ctx, val, input, output);
    }
  }, val);
}

bool Spec::Pattern::processMember(const Context& ctx, std::string_view name, const rapidjson::Value& member, rapidjson::Document& output) const {
  auto on_exit = ctx.log([&] (const auto& logger) {
    logger->log_trace("Processing member '{}' of {}", name, ctx.path());
  }, [&] (const auto& logger) {
    logger->log_trace("Finished processing member '{}' of {}", name, ctx.path());
  });
  if (auto it = literal_indices.find(std::string{name}); it != literal_indices.end()) {
    // literal is matched
    Context new_ctx = ctx.extend({name}, &member);
    process(std::get<2>(literals.at(it->second)), new_ctx, member, output);
    return true;
  }
  for (auto& templ : templates) {
    if (templ.first.eval(ctx) == name) {
      Context new_ctx = ctx.extend({name}, &member);
      process(templ.second, new_ctx, member, output);
      return true;
    }
  }
  for (auto& reg : regexes) {
    if (auto matches = reg.first.match(name)) {
      Context new_ctx = ctx.extend(matches.value(), &member);
      process(reg.second, new_ctx, member, output);
      return true;
    }
  }
  return false;
}

void Spec::Pattern::processArray(const Context& ctx, const rapidjson::Value &input, rapidjson::Document &output) const {
  gsl_Expects(input.IsArray());
  Context sub_ctx = ctx;
  for (auto& [key, numeric_key, value] : literals) {
    if (numeric_key && numeric_key.value() < input.GetArray().Size()) {
      if (processMember(sub_ctx, key, input[gsl::narrow<rapidjson::SizeType>(numeric_key.value())], output)) {
        ++sub_ctx.match_count;
      }
    }
  }
  for (rapidjson::SizeType  i = 0; i < input.GetArray().Size(); ++i) {
    if (literal_indices.contains(std::to_string(i))) {
      continue;
    }
    if (processMember(sub_ctx, std::to_string(i), input[i], output)) {
      ++sub_ctx.match_count;
    }
  }
}

void Spec::Pattern::processObject(const Context& ctx, const rapidjson::Value &input, rapidjson::Document &output) const {
  gsl_Expects(input.IsObject());
  Context sub_ctx = ctx;
  for (auto& [key, numeric_key, value] : literals) {
    if (input.GetObject().HasMember(key)) {
      if (processMember(sub_ctx, key, input[key], output)) {
        ++sub_ctx.match_count;
      }
    }
  }
  for (auto& [name, member] : input.GetObject()) {
    if (literal_indices.contains(std::string{name.GetString(), name.GetStringLength()})) {
      continue;
    }
    if (processMember(sub_ctx, std::string_view{name.GetString(), name.GetStringLength()}, member, output)) {
      ++sub_ctx.match_count;
    }
  }
}

void Spec::Pattern::process(const Context& ctx, const rapidjson::Value &input, rapidjson::Document &output) const {
  auto on_exit = ctx.log([&] (const auto& logger) {
    logger->log_trace("Processing node at {}", ctx.path());
  }, [&] (const auto& logger) {
    logger->log_trace("Finished processing node at {}", ctx.path());
  });
  for (auto& [val_ref, dest] : values) {
    auto& [idx, path] = val_ref;
    auto* target = ctx.find(idx);
    if (!target) {
      throw Exception(GENERAL_EXCEPTION, fmt::format("Could not find parent node at offset {} for {}", idx, ctx.path()));
    }
    if (!target->node) {
      return;
    }
    if (auto value = resolvePath(ctx, target->path(), *target->node, path)) {
      Context sub_ctx = ctx.extend(ctx.matches, ctx.node);
      process(dest, sub_ctx, value.value(), output);
    } else {
      ctx.log([&, path_ptr = &path] (const auto& logger) {
        auto path_str = toString(ctx, *path_ptr);
        logger->log_trace("Failed to resolve value path {} (evaled as {}) at {} (triggered from {}): {}", path_str.first, path_str.second, target->path(), ctx.path(), value.error());
      }, [] (const auto&) {});
      // pass, non-existent member is not an error
    }
  }
  for (auto& [key, dest] : keys) {
    auto key_str = ctx.find(key.first)->matches.at(key.second);
    putValue(ctx.extend({key_str}, nullptr), dest,
        rapidjson::Value{key_str.data(), gsl::narrow<rapidjson::SizeType>(key_str.size()), output.GetAllocator()}, output);  // NOLINT(bugprone-suspicious-stringview-data-usage)
  }
  for (auto& [value, dest] : defaults) {
    Context sub_ctx = ctx.extend({value}, nullptr);
    putValue(sub_ctx, dest, rapidjson::Value{value.data(), gsl::narrow<rapidjson::SizeType>(value.size()), output.GetAllocator()}, output);
  }
  if (input.IsArray()) {
    processArray(ctx, input, output);
  } else if (input.IsObject()) {
    processObject(ctx, input, output);
  } else if (input.IsString()) {
    processMember(ctx, std::string_view{input.GetString(), input.GetStringLength()}, rapidjson::Value{}, output);
  } else if (input.IsUint64()) {
    processMember(ctx, std::to_string(input.GetUint64()), rapidjson::Value{}, output);
  } else if (input.IsInt64()) {
    processMember(ctx, std::to_string(input.GetInt64()), rapidjson::Value{}, output);
  } else if (input.IsDouble()) {
    processMember(ctx, std::to_string(input.GetDouble()), rapidjson::Value{}, output);
  } else if (input.IsBool()) {
    processMember(ctx, input.GetBool() ? "true" : "false", rapidjson::Value{}, output);
  }
}

nonstd::expected<rapidjson::Document, std::string> Spec::process(const rapidjson::Value &input, std::shared_ptr<core::logging::Logger> logger) const {
  rapidjson::Document output;
  try {
    value_->process(Context{.matches = {"root"}, .node = &input, .logger = std::move(logger)}, input, output);
    return output;
  } catch (const std::exception& ex) {
    return nonstd::make_unexpected(ex.what());
  }
}

}  // namespace org::apache::nifi::minifi::utils::jolt
