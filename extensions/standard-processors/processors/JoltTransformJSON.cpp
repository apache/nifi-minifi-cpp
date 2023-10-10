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

#include "JoltTransformJSON.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

void JoltTransformJSON::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void JoltTransformJSON::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*session_factory*/) {
  gsl_Expects(context);
  transform_ = utils::parseEnumProperty<jolt_transform_json::JoltTransform>(*context, JoltTransform);
  const std::string spec_str = utils::getRequiredPropertyOrThrow(*context, JoltSpecification.name);
  if (auto spec = Spec::parse(spec_str)) {
    spec_ = std::move(spec.value());
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("The value of '{}' is not a valid jolt specification: {}", JoltSpecification.name, spec.error()));
  }
}

void JoltTransformJSON::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  gsl_Expects(context && session && spec_);
  auto flowfile = session->get();
  if (!flowfile) {
    context->yield();
    return;
  }

  auto content = session->readBuffer(flowfile);
  rapidjson::Document input;
  rapidjson::ParseResult parse_result = input.Parse(reinterpret_cast<const char*>(content.buffer.data()), content.buffer.size());
  if (!parse_result) {
    session->transfer(flowfile, Failure);
    return;
  }

  if (auto result = spec_->process(input)) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    result.value().Accept(writer);
    session->writeBuffer(flowfile, std::span<const char>(buffer.GetString(), buffer.GetSize()));
    session->transfer(flowfile, Success);
  } else {
    session->transfer(flowfile, Failure);
  }
}

bool JoltTransformJSON::Spec::Template::check(std::string_view str) {
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

nonstd::expected<JoltTransformJSON::Spec::Template, std::string> JoltTransformJSON::Spec::Template::parse(std::string_view str, const std::string& escapables) {
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
  for (size_t ch_idx = 0; ch_idx <= str.size(); ++ch_idx) {
    std::optional<char> ch;
    if (ch_idx < str.size()) {
      ch = str[ch_idx];
    }
    switch (state) {
      case State::Plain: {
        if (ch == '\\') {
          state = State::Escaped;
        } else if (ch == '&') {
          references.push_back({});
          fragments.push_back({});
          state = State::Template;
        } else if (ch) {
          fragments.back() += ch.value();
        }
        break;
      }
      case State::Escaped: {
        if (!ch) {
          return nonstd::make_unexpected("Unterminated escape sequence");
        }
        if (!(ch == '\\' || ch == '&' || std::find(escapables.begin(), escapables.end(), ch.value()) != escapables.end())) {
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
          --ch_idx;
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
          --ch_idx;
        }
        break;
      }
      case State::CanonicalTemplate: {
        if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::ParentIndex;
        } else {
          return nonstd::make_unexpected(fmt::format("Expected an index at {}", ch_idx));
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
          return nonstd::make_unexpected(fmt::format("Invalid character at {}, expected digit, comma or close parenthesis", ch_idx));
        }
        break;
      }
      case State::NextIndex: {
        if (ch && std::isdigit(static_cast<unsigned char>(ch.value()))) {
          target.clear();
          target += ch.value();
          state = State::MatchIndex;
        } else {
          return nonstd::make_unexpected(fmt::format("Expected an index at {}", ch_idx));
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
          return nonstd::make_unexpected(fmt::format("Invalid character at {}, expected digit or close parenthesis", ch_idx));
        }
        break;
      }
    }
  }

  gsl_Assert(state == State::Plain);
  return Template{std::move(fragments), std::move(references)};
}

bool JoltTransformJSON::Spec::Regex::check(std::string_view str) {
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

nonstd::expected<JoltTransformJSON::Spec::Regex, std::string> JoltTransformJSON::Spec::Regex::parse(std::string_view str, const std::string& escapables) {
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
        if (!(ch == '\\' || ch == '&' || std::find(escapables.begin(), escapables.end(), ch.value()) != escapables.end())) {
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

std::string JoltTransformJSON::Spec::Template::eval(const Context& ctx) const {
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

std::optional<std::vector<std::string_view>> JoltTransformJSON::Spec::Regex::match(std::string_view str) const {
  std::vector<std::string_view> matches;
  matches.push_back(str);
  auto it = str.begin();
  for (size_t idx = 0; idx < fragments.size(); ++idx) {
    auto& frag = fragments[idx];
    auto next_it = std::search(it, str.end(), frag.begin(), frag.end());
    if (next_it == str.end()) {
      return std::nullopt;
    }
    if (idx == 0 && next_it != str.begin()) {
      // the first literal fragment must start at the beginning of the string
      return std::nullopt;
    }
    matches.push_back({it, next_it});
    it = next_it + frag.size();
  }
  if (it != str.end()) {
    return std::nullopt;
  }
  return matches;
}

namespace {

JoltTransformJSON::Spec::Destinations parseDestinations(const JoltTransformJSON::Spec::Context& ctx, const rapidjson::Value& val);

JoltTransformJSON::Spec::Pattern::Value parseValue(const JoltTransformJSON::Spec::Context& ctx, const rapidjson::Value& val);

std::unique_ptr<JoltTransformJSON::Spec::Pattern> parseMap(const JoltTransformJSON::Spec::Context& ctx, const rapidjson::Value& val) {
  if (!val.IsObject()) {
    throw Exception(GENERAL_EXCEPTION, fmt::format("Expected a map at '{}'", ctx.path()));
  }
  auto map = std::make_unique<JoltTransformJSON::Spec::Pattern>();

  for (auto it = val.MemberBegin(); it != val.MemberEnd(); ++it) {
    std::string_view name{it->name.GetString(), it->name.GetStringLength()};
    if (name == "@") {
      JoltTransformJSON::Spec::Context sub_ctx{.parent = &ctx, .matches = {name}};
      map->self = parseDestinations(sub_ctx, it->value);
    } else if (name == "$") {
      JoltTransformJSON::Spec::Context sub_ctx{.parent = &ctx, .matches = {name}};
      map->key = parseDestinations(sub_ctx, it->value);
    } else {
      const bool is_template = JoltTransformJSON::Spec::Template::check(name);
      const bool is_regex = JoltTransformJSON::Spec::Regex::check(name);
      if (is_template && is_regex) {
        throw Exception(GENERAL_EXCEPTION, "Pattern cannot contain both & and *");
      }
      if (is_template) {
        if (auto templ = JoltTransformJSON::Spec::Template::parse(name, "*")) {
          // dry eval so we can check if the references refer to valid substrings
          (void)templ.value().eval(ctx);
          JoltTransformJSON::Spec::Context sub_ctx{.parent = &ctx, .matches = {name}};
          map->templates.insert({templ.value(), parseValue(sub_ctx, it->value)});
        } else {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Error while parsing key template at {}: {}", ctx.path(), templ.error()));
        }
      } else if (is_regex) {
        if (auto reg = JoltTransformJSON::Spec::Regex::parse(name, "&")) {
          JoltTransformJSON::Spec::Context sub_ctx{.parent = &ctx, .matches = {name}};
          sub_ctx.matches.resize(reg.value().size());
          map->regexes.insert({reg.value(), parseValue(sub_ctx, it->value)});
        } else {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Error while parsing key regex at {}: {}", ctx.path(), reg.error()));
        }
      } else {
        JoltTransformJSON::Spec::Context sub_ctx{.parent = &ctx, .matches = {name}};
        map->literals.insert({std::string{name}, parseValue(sub_ctx, it->value)});
      }
    }
  }
  return map;
}

// dot-delimited list of templates
std::vector<JoltTransformJSON::Spec::Template> parseDestination(const JoltTransformJSON::Spec::Context& ctx, const rapidjson::Value& val) {
  if (!val.IsString()) {
    throw Exception(GENERAL_EXCEPTION, fmt::format("Expected a string or array of strings at '{}'", ctx.path()));
  }

  enum class State {
    Plain,
    Escaped
  };

  std::vector<JoltTransformJSON::Spec::Template> result;
  std::string_view str{val.GetString(), val.GetStringLength()};
  State state = State::Plain;
  size_t segment_begin = 0;
  for (size_t idx = 0; idx <= str.size(); ++idx) {
    std::optional<char> ch;
    if (idx < str.size()) {
      ch = str[idx];
    }
    switch (state) {
      case State::Plain: {
        if (ch == '\\') {
          state = State::Escaped;
        } else if (!ch || ch == '.') {
          auto member = str.substr(segment_begin, idx - segment_begin);
          if (auto templ = JoltTransformJSON::Spec::Template::parse(member, ".")) {
            // dry eval to verify that references are valid
            (void)templ.value().eval(ctx);
            result.push_back(templ.value());
          } else {
            throw Exception(GENERAL_EXCEPTION, fmt::format("Failed to parse member as template '{}' at {}", member, ctx.path()));
          }
          segment_begin = idx + 1;
        }
        break;
      }
      case State::Escaped: {
        if (!ch) {
          throw Exception(GENERAL_EXCEPTION, "Unterminated escape sequence in destination");
        }
        if (!(ch == '\\' || ch == '.' || ch == '&')) {
          throw Exception(GENERAL_EXCEPTION, fmt::format("Unknown escape sequence in destination '\\{}'", ch.value()));
        }
        break;
      }
    }
  }

  gsl_Assert(state == State::Plain);

  return result;
}

JoltTransformJSON::Spec::Destinations parseDestinations(const JoltTransformJSON::Spec::Context& ctx, const rapidjson::Value& val) {
  JoltTransformJSON::Spec::Destinations res;
  if (val.IsArray()) {
    for (rapidjson::SizeType i = 0; i < val.GetArray().Size(); ++i) {
      std::string idx_str = std::to_string(i);
      res.push_back(parseDestination(ctx, val.GetArray()[i]));
    }
  } else {
    res.push_back(parseDestination(ctx, val));
  }
  return res;
}


JoltTransformJSON::Spec::Pattern::Value parseValue(const JoltTransformJSON::Spec::Context& ctx, const rapidjson::Value& val) {
  if (val.IsObject()) {
    return parseMap(ctx, val);
  }
  return parseDestinations(ctx, val);
}

void putValue(const JoltTransformJSON::Spec::Context& ctx, const std::vector<JoltTransformJSON::Spec::Template>& dest, const rapidjson::Value& val, rapidjson::Document& output) {
  std::reference_wrapper<rapidjson::Value> target = output;
  for (auto& templ : dest) {
    auto member = templ.eval(ctx);
    if (templ.type == JoltTransformJSON::Spec::Template::Type::INDEX) {
      if (!target.get().IsArray()) {
        if (!target.get().IsNull()) {
          throw Exception(GENERAL_EXCEPTION, "Cannot write based on index into non-array");
        }
        target.get().SetArray();
      }
      size_t idx = std::stoull(member);
      target.get().Reserve(idx + 1, output.GetAllocator());
      for (size_t arr_idx = target.get().Size(); arr_idx <= idx; ++arr_idx) {
        target.get().PushBack(rapidjson::Value{}, output.GetAllocator());
      }
      target = target.get()[idx];
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

void putValue(const JoltTransformJSON::Spec::Context& ctx, const JoltTransformJSON::Spec::Destinations& destinations, const rapidjson::Value& val, rapidjson::Document& output) {
  for (auto& dest : destinations) {
    putValue(ctx, dest, val, output);
  }
}

}  // namespace

nonstd::expected<JoltTransformJSON::Spec, std::string> JoltTransformJSON::Spec::parse(std::string_view str) {
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(str.data(), str.length());
  if (!res) {
    return nonstd::make_unexpected(fmt::format("{} at {}", rapidjson::GetParseError_En(res.Code()), res.Offset()));
  }
  try {
    Spec::Context ctx{.matches = {"root"}};
    return Spec{parseMap(ctx, doc)};
  } catch (const std::exception& ex) {
    return nonstd::make_unexpected(ex.what());
  }
}

void JoltTransformJSON::Spec::Pattern::process(const Value& val, const Context& ctx, const rapidjson::Value& input, rapidjson::Document& output) {
  std::visit([&] (auto& val) {
    if constexpr (std::is_same_v<std::decay_t<decltype(val)>, std::unique_ptr<Pattern>>) {
      val->process(ctx, input, output);
    } else {
      putValue(ctx, val, input, output);
    }
  }, val);
}

void JoltTransformJSON::Spec::Pattern::processMember(const Context& ctx, const rapidjson::Value::Member& member, rapidjson::Document& output) const {
  std::string_view name{member.name.GetString(), member.name.GetStringLength()};
  if (auto it = literals.find(std::string{name}); it != literals.end()) {
    // literal is matched
    Context new_ctx{.parent = &ctx, .matches = {name}};
    process(it->second, new_ctx, member.value, output);
    return;
  }
  for (auto& templ : templates) {
    if (templ.first.eval(ctx) == name) {
      Context new_ctx{.parent = &ctx, .matches = {name}};
      process(templ.second, new_ctx, member.value, output);
      return;
    }
  }
  for (auto& reg : regexes) {
    if (auto matches = reg.first.match(name)) {
      Context new_ctx{.parent = &ctx, .matches = matches.value()};
      process(reg.second, new_ctx, member.value, output);
      return;
    }
  }
}

void JoltTransformJSON::Spec::Pattern::process(const Context& ctx, const rapidjson::Value &input, rapidjson::Document &output) const {
  if (self) {
    putValue(ctx, self.value(), input, output);
  }
  if (key) {
    auto key_str = ctx.find(0)->matches.at(0);
    putValue(ctx, key.value(), rapidjson::Value{key_str.data(), gsl::narrow<rapidjson::SizeType>(key_str.size())}, output);
  }
  if (!input.IsObject()) {
    return;
  }
  for (auto& member : input.GetObject()) {
    processMember(ctx, member, output);
  }
}

nonstd::expected<rapidjson::Document, std::string> JoltTransformJSON::Spec::process(const rapidjson::Value &input) const {
  rapidjson::Document output;
  try {
    value_->process(Context{.matches = {"root"}}, input, output);
    return output;
  } catch (const std::exception& ex) {
    return nonstd::make_unexpected(ex.what());
  }
}

REGISTER_RESOURCE(JoltTransformJSON, Processor);

}  // namespace org::apache::nifi::minifi::processors
