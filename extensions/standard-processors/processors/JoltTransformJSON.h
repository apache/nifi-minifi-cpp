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

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"
#include "utils/Searcher.h"

namespace org::apache::nifi::minifi::processors::jolt_transform_json {
enum class JoltTransform {
  SHIFT
};
}  // namespace org::apache::nifi::minifi::extensions::gcp

namespace magic_enum::customize {
using JoltTransform = org::apache::nifi::minifi::processors::jolt_transform_json::JoltTransform;

template <>
constexpr customize_t enum_name<JoltTransform>(JoltTransform value) noexcept {
  switch (value) {
    case JoltTransform::SHIFT:
      return "Shift";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

class JoltTransformJSON : public core::Processor {
 public:
  explicit JoltTransformJSON(std::string_view name, const utils::Identifier& uuid = {})
      : Processor(name, uuid) {}


  EXTENSIONAPI static constexpr const char* Description = "Applies a list of Jolt specifications to the flowfile JSON payload. A new FlowFile is created "
      "with transformed content and is routed to the 'success' relationship. If the JSON transform "
      "fails, the original FlowFile is routed to the 'failure' relationship.";

  EXTENSIONAPI static constexpr auto JoltTransform = core::PropertyDefinitionBuilder<magic_enum::enum_count<jolt_transform_json::JoltTransform>()>::createProperty("Jolt Transformation DSL")
      .withDescription("Specifies the Jolt Transformation that should be used with the provided specification.")
      .withDefaultValue(magic_enum::enum_name(jolt_transform_json::JoltTransform::SHIFT))
      .withAllowedValues(magic_enum::enum_names<jolt_transform_json::JoltTransform>())
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto JoltSpecification = core::PropertyDefinitionBuilder<>::createProperty("Jolt Specification")
      .withDescription("Jolt Specification for transformation of JSON data. The value for this property may be the text of a Jolt specification "
          "or the path to a file containing a Jolt specification. 'Jolt Specification' must be set, or "
          "the value is ignored if the Jolt Sort Transformation is selected.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 2>{
      JoltTransform,
      JoltSpecification
  };

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "The FlowFile with transformed content will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "If a FlowFile fails processing for any reason (for example, the FlowFile is not valid JSON), it will be routed to this relationship"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* session_factory) override;
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;
  void initialize() override;

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

      ::gsl::final_action<std::function<void()>> log(std::function<void(std::shared_ptr<core::logging::Logger>)> on_enter, std::function<void(std::shared_ptr<core::logging::Logger>)> on_exit) const {
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
      static nonstd::expected<Template, std::string> parse(std::string_view str, std::string_view escapables) {
        if (auto res = parse(str.begin(), str.end(), escapables)) {
          if (res->second != str.end()) {
            return nonstd::make_unexpected("Failed to fully parse template");
          }
          return {std::move(res->first)};
        } else {
          return nonstd::make_unexpected(std::move(res.error()));
        }
      }
      static nonstd::expected<std::pair<Template, It>, std::string> parse(It begin, It end, std::string_view escapables);

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
        full = utils::StringUtils::join("*", fragments);
      }
      // checks if the string is definitely a regex (i.e. has an unescaped '*' char)
      static bool check(std::string_view str);
      static nonstd::expected<Regex, std::string> parse(std::string_view str, std::string_view escapables);

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
      bool processMember(const Context& ctx, std::string_view key, const rapidjson::Value& member, rapidjson::Document& output) const;

      std::unordered_map<std::string, size_t> literal_indices;
      std::vector<std::tuple<std::string, std::optional<size_t>, Value>> literals;

      std::map<Template, Value> templates;  // '&'
      std::map<Regex, Value> regexes;  // '*'
      std::vector<std::pair<ValueRef, Value>> values;  // '@', '@1', '@(1,key.path)'
      std::map<std::pair<size_t, size_t>, Destinations> keys;  // '$', '$(0,1)'
      std::map<std::string, Destinations> defaults; // #thing: a.b
    };

    static nonstd::expected<Spec, std::string> parse(std::string_view str);

    nonstd::expected<rapidjson::Document, std::string> process(const rapidjson::Value& input, std::shared_ptr<core::logging::Logger> logger = {}) const;

   private:
    explicit Spec(std::unique_ptr<Pattern> value): value_(std::move(value)) {}

    std::unique_ptr<Pattern> value_;
  };

 private:
  jolt_transform_json::JoltTransform transform_;
  std::optional<Spec> spec_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<JoltTransformJSON>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
