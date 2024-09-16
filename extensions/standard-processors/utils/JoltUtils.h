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
#include <string_view>
#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <memory>
#include <compare>
#include <concepts>

#include "core/logging/Logger.h"
#include "utils/gsl.h"
#include "rapidjson/document.h"
#include "utils/expected.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::jolt {

class Spec {
 public:
  using It = std::string_view::const_iterator;

  struct Context {
   public:
    const Context* parent{nullptr};

    std::string path() const {
      std::string res;
      if (parent) {
        res = parent->path();
      }
      res.append("/").append(matches.at(0));
      return res;
    }

    const Context* find(size_t idx) const {
      if (idx == 0) return this;
      if (parent) return parent->find(idx - 1);
      return nullptr;
    }

    template<std::invocable<std::shared_ptr<core::logging::Logger>> OnEnterFn, std::invocable<std::shared_ptr<core::logging::Logger>> OnExitFn>
    ::gsl::final_action<std::function<void()>> log(OnEnterFn on_enter, OnExitFn on_exit) const {
      if (logger) {
        on_enter(logger);
        return gsl::finally<std::function<void()>>([on_exit, logger = logger] {
          on_exit(logger);
        });
      }
      if (parent) {
        return parent->log(on_enter, on_exit);
      }
      return gsl::finally<std::function<void()>>([]{});
    }

    Context extend(std::vector<std::string_view> sub_matches, const rapidjson::Value* sub_node) const {
      return {.parent = this, .matches = std::move(sub_matches), .node = sub_node, .match_count = 0, .logger = logger};
    }

    std::vector<std::string_view> matches;
    const rapidjson::Value* node{nullptr};
    size_t match_count{0};
    std::shared_ptr<core::logging::Logger> logger;
  };

  class Template {
   public:
    Template(std::vector<std::string> frags, std::vector<std::pair<size_t, size_t>> refs) : fragments(std::move(frags)), references(std::move(refs)) {
      gsl_Expects(fragments.size() == references.size() + 1);  // implies that fragments is non-empty
      full = fragments.front();
      for (size_t idx = 0; idx < references.size(); ++idx) {
        full
            .append("&(")
            .append(std::to_string(references[idx].first))
            .append(",")
            .append(std::to_string(references[idx].second))
            .append(")")
            .append(fragments[idx + 1]);
      }
    }

    // checks if the string is definitely a template (i.e. has an unescaped '&' char)
    static bool check(std::string_view str);
    static nonstd::expected<Template, std::string> parse(std::string_view str) {
      if (auto res = parse(str.begin(), str.end())) {
        if (res->second != str.end()) {
          return nonstd::make_unexpected("Failed to fully parse template");
        }
        return {std::move(res->first)};
      } else {
        return nonstd::make_unexpected(std::move(res.error()));
      }
    }
    static nonstd::expected<std::pair<Template, It>, std::string> parse(It begin, It end);

    std::string eval(const Context& ctx) const;

    auto operator<=>(const Template& other) const {
      return full <=> other.full;
    }

    auto operator==(const Template& other) const {
      return full == other.full;
    }

    bool empty() const {
      return fragments.size() == 1 && fragments.front().empty();
    }

    std::vector<std::string> fragments;
    std::vector<std::pair<size_t, size_t>> references;

    std::string full;
  };

  class Regex {
   public:
    explicit Regex(std::vector<std::string> frags) : fragments(std::move(frags)) {
      gsl_Expects(!fragments.empty());
      full = utils::string::join("*", fragments);
    }
    // checks if the string is definitely a regex (i.e. has an unescaped '*' char)
    static bool check(std::string_view str);
    static nonstd::expected<Regex, std::string> parse(std::string_view str);

    std::optional<std::vector<std::string_view>> match(std::string_view str) const;

    auto operator<=>(const Regex& other) const {
      return full <=> other.full;
    }

    auto operator==(const Template& other) const {
      return full == other.full;
    }

    // the size of the match vector on a successful match
    // e.g. "A*B*" matching on "A12B34" will return ["A12B34", "12", "34"]
    size_t size() const {
      return fragments.size();
    }

   private:
    std::vector<std::string> fragments;
    std::string full;
  };

  enum class MemberType {
    FIELD,
    INDEX
  };

  using Path = std::vector<std::pair<Template, MemberType>>;
  using ValueRef = std::pair<size_t, Path>;
  using MatchingIndex = size_t;
  using Destination = std::vector<std::pair<std::variant<Template, ValueRef, MatchingIndex>, MemberType>>;
  using Destinations = std::vector<Destination>;

  struct Pattern {
    using Value = std::variant<std::unique_ptr<Pattern>, Destinations>;

    static void process(const Value& val, const Context& ctx, const rapidjson::Value& input, rapidjson::Document& output);

    void process(const Context& ctx, const rapidjson::Value& input, rapidjson::Document& output) const;
    void processArray(const Context& ctx, const rapidjson::Value &input, rapidjson::Document &output) const;
    void processObject(const Context& ctx, const rapidjson::Value &input, rapidjson::Document &output) const;
    bool processMember(const Context& ctx, std::string_view name, const rapidjson::Value& member, rapidjson::Document& output) const;

    std::unordered_map<std::string, size_t> literal_indices;
    std::vector<std::tuple<std::string, std::optional<size_t>, Value>> literals;

    std::map<Template, Value> templates;  // '&'
    std::map<Regex, Value> regexes;  // '*'
    std::vector<std::pair<ValueRef, Value>> values;  // '@', '@1', '@(1,key.path)'
    std::map<std::pair<size_t, size_t>, Destinations> keys;  // '$', '$(0,1)'
    std::map<std::string, Destinations> defaults;  // #thing: a.b
  };

  static nonstd::expected<Spec, std::string> parse(std::string_view str, std::shared_ptr<core::logging::Logger> logger = {});

  nonstd::expected<rapidjson::Document, std::string> process(const rapidjson::Value& input, std::shared_ptr<core::logging::Logger> logger = {}) const;

 private:
  explicit Spec(std::unique_ptr<Pattern> value): value_(std::move(value)) {}

  std::unique_ptr<Pattern> value_;
};

}  // namespace org::apache::nifi::minifi::utils::jolt
