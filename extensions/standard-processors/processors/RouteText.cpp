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

#include "RouteText.h"

#include <algorithm>
#include <map>
#include <vector>
#include <utility>

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "io/StreamPipe.h"
#include "core/logging/LoggerFactory.h"
#include "range/v3/view/transform.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/tail.hpp"
#include "range/v3/view/join.hpp"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OptionalUtils.h"
#include "utils/Searcher.h"

namespace org::apache::nifi::minifi::processors {

RouteText::RouteText(std::string_view name, const utils::Identifier& uuid)
    : core::ProcessorImpl(name, uuid), logger_(core::logging::LoggerFactory<RouteText>::getLogger(uuid)) {}

void RouteText::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void RouteText::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  routing_ = utils::parseEnumProperty<route_text::Routing>(context, RoutingStrategy);
  matching_ = utils::parseEnumProperty<route_text::Matching>(context, MatchingStrategy);
  context.getProperty(TrimWhitespace, trim_);
  case_policy_ = context.getProperty<bool>(IgnoreCase).value_or(false) ? route_text::CasePolicy::IGNORE_CASE : route_text::CasePolicy::CASE_SENSITIVE;
  group_regex_ = context.getProperty(GroupingRegex) | utils::transform([] (const auto& str) {return utils::Regex(str);});
  segmentation_ = utils::parseEnumProperty<route_text::Segmentation>(context, SegmentationStrategy);
  context.getProperty(GroupingFallbackValue, group_fallback_);
}

class RouteText::ReadCallback {
  using Fn = std::function<void(Segment)>;

 public:
  ReadCallback(route_text::Segmentation segmentation, size_t file_size, Fn&& fn)
    : segmentation_(segmentation), file_size_(file_size), fn_(std::move(fn)) {}

  int64_t operator()(const std::shared_ptr<io::InputStream>& stream) const {
    std::vector<std::byte> buffer;
    buffer.resize(file_size_);
    size_t ret = stream->read(buffer);
    if (io::isError(ret)) {
      return -1;
    }
    if (ret != file_size_) {
      throw Exception(PROCESS_SESSION_EXCEPTION, "Couldn't read whole flowfile content");
    }
    std::string_view content{reinterpret_cast<const char*>(buffer.data()), buffer.size()};
    switch (segmentation_) {
      case route_text::Segmentation::FULL_TEXT: {
        fn_({content, 0});
        return gsl::narrow<int64_t>(content.length());
      }
      case route_text::Segmentation::PER_LINE: {
        // 1-based index as in nifi
        size_t segment_idx = 1;
        std::string_view::size_type curr = 0;
        while (curr < content.length()) {
          // find beginning of next line
          std::string_view::size_type next_line = content.find('\n', curr);

          if (next_line == std::string_view::npos) {
            fn_({content.substr(curr), segment_idx});
          } else {
            // include newline character to be in-line with nifi semantics
            ++next_line;
            fn_({content.substr(curr, next_line - curr), segment_idx});
          }
          curr = next_line;
          ++segment_idx;
        }
        return gsl::narrow<int64_t>(content.length());
      }
    }
    throw Exception(PROCESSOR_EXCEPTION, "Unknown segmentation strategy");
  }

 private:
  route_text::Segmentation segmentation_;
  size_t file_size_;
  Fn fn_;
};

class RouteText::MatchingContext {
  struct CaseAwareHash {
    explicit CaseAwareHash(route_text::CasePolicy policy): policy_(policy) {}
    size_t operator()(char ch) const {
      if (policy_ == route_text::CasePolicy::CASE_SENSITIVE) {
        return static_cast<size_t>(ch);
      }
      return std::hash<int>{}(std::tolower(static_cast<unsigned char>(ch)));
    }

   private:
    route_text::CasePolicy policy_;
  };

  struct CaseAwareEq {
    explicit CaseAwareEq(route_text::CasePolicy policy): policy_(policy) {}
    bool operator()(char a, char b) const {
      if (policy_ == route_text::CasePolicy::CASE_SENSITIVE) {
        return a == b;
      }
      return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    }

   private:
    route_text::CasePolicy policy_;
  };

 public:
  MatchingContext(core::ProcessContext& process_context, std::shared_ptr<core::FlowFile> flow_file, route_text::CasePolicy case_policy)
    : process_context_(process_context),
      flow_file_(std::move(flow_file)),
      case_policy_(case_policy) {}

  const utils::Regex& getRegexProperty(const core::Property& prop) {
    auto it = regex_values_.find(prop.getName());
    if (it != regex_values_.end()) {
      return it->second;
    }
    std::string value;
    if (!process_context_.getDynamicProperty(prop, value, flow_file_.get())) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }
    std::vector<utils::Regex::Mode> flags;
    if (case_policy_ == route_text::CasePolicy::IGNORE_CASE) {
      flags.push_back(utils::Regex::Mode::ICASE);
    }
    return (regex_values_[prop.getName()] = utils::Regex(value, flags));
  }

  const std::string& getStringProperty(const core::Property& prop) {
    auto it = string_values_.find(prop.getName());
    if (it != string_values_.end()) {
      return it->second;
    }
    std::string value;
    if (!process_context_.getDynamicProperty(prop, value, flow_file_.get())) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }
    return (string_values_[prop.getName()] = value);
  }

  const auto& getSearcher(const core::Property& prop) {
    auto it = searcher_values_.find(prop.getName());
    if (it != searcher_values_.end()) {
      return it->second.searcher_;
    }
    std::string value;
    if (!process_context_.getDynamicProperty(prop, value, flow_file_.get())) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }

    return searcher_values_.emplace(
        std::piecewise_construct, std::forward_as_tuple(prop.getName()),
        std::forward_as_tuple(value, case_policy_)).first->second.searcher_;
  }

  core::ProcessContext& process_context_;
  std::shared_ptr<core::FlowFile> flow_file_;
  route_text::CasePolicy case_policy_;

  std::map<std::string, std::string> string_values_;
  std::map<std::string, utils::Regex> regex_values_;

  struct OwningSearcher {
    OwningSearcher(std::string str, route_text::CasePolicy case_policy)
      : str_(std::move(str)), searcher_(str_.cbegin(), str_.cend(), CaseAwareHash{case_policy}, CaseAwareEq{case_policy}) {}
    OwningSearcher(const OwningSearcher&) = delete;
    OwningSearcher(OwningSearcher&&) = delete;
    OwningSearcher& operator=(const OwningSearcher&) = delete;
    OwningSearcher& operator=(OwningSearcher&&) = delete;
    ~OwningSearcher() = default;

    std::string str_;
    utils::Searcher<std::string::const_iterator, CaseAwareHash, CaseAwareEq> searcher_;
  };

  std::map<std::string, OwningSearcher> searcher_values_;
};

namespace {
struct Route {
  core::Relationship relationship_;
  std::optional<std::string> group_name_;

  bool operator<(const Route& other) const {
    return std::tie(relationship_, group_name_) < std::tie(other.relationship_, other.group_name_);
  }
};
}  // namespace

void RouteText::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  std::map<Route, std::string> flow_file_contents;

  MatchingContext matching_context(context, flow_file, case_policy_);

  ReadCallback callback(segmentation_, flow_file->getSize(), [&] (Segment segment) {
    std::string_view original_value = segment.value_;
    std::string_view preprocessed_value = preprocess(segment.value_);

    if (matching_ != route_text::Matching::EXPRESSION) {
      // an Expression has access to the raw segment like in nifi
      // all others use the preprocessed_value
      segment.value_ = preprocessed_value;
    }

    // group extraction always uses the preprocessed
    auto group = getGroup(preprocessed_value);
    switch (routing_) {
      case route_text::Routing::ALL: {
        if (std::all_of(dynamic_properties_.cbegin(), dynamic_properties_.cend(), [&] (const auto& prop) {
          return matchSegment(matching_context, segment, prop.second);
        })) {
          flow_file_contents[{Matched, group}] += original_value;
        } else {
          flow_file_contents[{Unmatched, group}] += original_value;
        }
        return;
      }
      case route_text::Routing::ANY: {
        if (std::any_of(dynamic_properties_.cbegin(), dynamic_properties_.cend(), [&] (const auto& prop) {
          return matchSegment(matching_context, segment, prop.second);
        })) {
          flow_file_contents[{Matched, group}] += original_value;
        } else {
          flow_file_contents[{Unmatched, group}] += original_value;
        }
        return;
      }
      case route_text::Routing::DYNAMIC: {
        bool routed = false;
        for (const auto& [property_name, prop] : dynamic_properties_) {
          if (matchSegment(matching_context, segment, prop)) {
            flow_file_contents[{dynamic_relationships_[property_name], group}] += original_value;
            routed = true;
          }
        }
        if (!routed) {
          flow_file_contents[{Unmatched, group}] += original_value;
        }
        return;
      }
    }
    throw Exception(PROCESSOR_EXCEPTION, "Unknown routing strategy");
  });
  session.read(flow_file, std::move(callback));

  for (const auto& [route, content] : flow_file_contents) {
    auto new_flow_file = session.create(flow_file.get());
    if (route.group_name_) {
      new_flow_file->setAttribute(GROUP_ATTRIBUTE_NAME, route.group_name_.value());
    }
    session.writeBuffer(new_flow_file, content);
    session.transfer(new_flow_file, route.relationship_);
  }

  session.transfer(flow_file, Original);
}

std::string_view RouteText::preprocess(std::string_view str) const {
  if (segmentation_ == route_text::Segmentation::PER_LINE) {
    // do not consider the trailing \r\n characters in order to conform to nifi
    auto len = str.find_last_not_of("\r\n");
    if (len != std::string_view::npos) {
      str = str.substr(0, len + 1);
    } else {
      str = "";
    }
  }
  if (trim_) {
    str = utils::string::trim(str);
  }
  return str;
}

namespace {
bool getDynamicPropertyWithOverrides(core::ProcessContext& context,
    const core::Property &property,
    std::string &value,
    core::FlowFile& flow_file,
    const std::map<std::string, std::string>& overrides) {
  std::map<std::string, std::optional<std::string>> original_attributes;
  for (const auto& [override_key, override_value] : overrides) {
    original_attributes[override_key] = flow_file.getAttribute(override_key);
    flow_file.setAttribute(override_key, override_value);
  }
  auto onExit = gsl::finally([&]{
    for (const auto& attr : original_attributes) {
      if (attr.second) {
        flow_file.setAttribute(attr.first, attr.second.value());
      } else {
        flow_file.removeAttribute(attr.first);
      }
    }
  });
  return context.getDynamicProperty(property, value, &flow_file);
}
}  // namespace

bool RouteText::matchSegment(MatchingContext& context, const Segment& segment, const core::Property& prop) const {
  switch (matching_) {
    case route_text::Matching::EXPRESSION: {
      std::map<std::string, std::string> variables;
      variables["segment"] = segment.value_;
      variables["segmentNo"] = std::to_string(segment.idx_);
      if (segmentation_ == route_text::Segmentation::PER_LINE) {
        // for nifi compatibility
        variables["line"] = segment.value_;
        variables["lineNo"] = std::to_string(segment.idx_);
      }
      std::string result;
      if (getDynamicPropertyWithOverrides(context.process_context_, prop, result, *context.flow_file_, variables)) {
        return utils::string::toBool(result).value_or(false);
      } else {
        throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
      }
    }
    case route_text::Matching::STARTS_WITH: {
      return utils::string::startsWith(segment.value_, context.getStringProperty(prop), case_policy_ == route_text::CasePolicy::CASE_SENSITIVE);
    }
    case route_text::Matching::ENDS_WITH: {
      return utils::string::endsWith(segment.value_, context.getStringProperty(prop), case_policy_ == route_text::CasePolicy::CASE_SENSITIVE);
    }
    case route_text::Matching::CONTAINS: {
      return std::search(segment.value_.begin(), segment.value_.end(), context.getSearcher(prop)) != segment.value_.end();
    }
    case route_text::Matching::EQUALS: {
      return utils::string::equals(segment.value_, context.getStringProperty(prop), case_policy_ == route_text::CasePolicy::CASE_SENSITIVE);
    }
    case route_text::Matching::CONTAINS_REGEX: {
      std::string segment_str = std::string(segment.value_);
      return utils::regexSearch(segment_str, context.getRegexProperty(prop));
    }
    case route_text::Matching::MATCHES_REGEX: {
      std::string segment_str = std::string(segment.value_);
      return utils::regexMatch(segment_str, context.getRegexProperty(prop));
    }
  }
  throw Exception(PROCESSOR_EXCEPTION, "Unknown matching strategy");
}

std::optional<std::string> RouteText::getGroup(const std::string_view& segment) const {
  if (!group_regex_) {
    return std::nullopt;
  }
  utils::SMatch match_result;
  std::string segment_str = std::string(segment);
  if (!utils::regexMatch(segment_str, match_result, group_regex_.value())) {
    return group_fallback_;
  }
  // unused capturing groups default to empty string
  auto to_string = [] (const auto& submatch) -> std::string {return submatch;};
  return ranges::views::tail(match_result)  // only join the capture groups
    | ranges::views::transform(to_string)
    | ranges::views::join(std::string_view(", "))
    | ranges::to<std::string>();
}

void RouteText::onDynamicPropertyModified(const core::Property& /*orig_property*/, const core::Property& new_property) {
  dynamic_properties_[new_property.getName()] = new_property;

  const auto static_relationships = RouteText::Relationships;
  std::vector<core::RelationshipDefinition> relationships(static_relationships.begin(), static_relationships.end());

  for (const auto& [property_name, prop] : dynamic_properties_) {
    core::RelationshipDefinition rel{property_name, "Dynamic Route"};
    dynamic_relationships_[property_name] = rel;
    relationships.push_back(rel);
    logger_->log_info("RouteText registered dynamic route '{}' with expression '{}'", property_name, prop.getValue().to_string());
  }

  setSupportedRelationships(relationships);
}

REGISTER_RESOURCE(RouteText, Processor);

}  // namespace org::apache::nifi::minifi::processors
