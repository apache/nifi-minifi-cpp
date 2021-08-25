/**
 * @file RouteText.cpp
 * TailFile class declaration
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

#include "RouteText.h"

#include <map>

#include "logging/LoggerConfiguration.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::processors {

const core::Property RouteText::RoutingStrategy(
    core::PropertyBuilder::createProperty("Routing Strategy")
    ->withDescription("Specifies how to determine which Relationship(s) to use when evaluating the segments "
                      "of incoming text against the 'Matching Strategy' and user-defined properties.")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(Routing::DYNAMIC))
    ->withAllowableValues<std::string>(Routing::values())
    ->build());

const core::Property RouteText::MatchingStrategy(
    core::PropertyBuilder::createProperty("Matching Strategy")
    ->withDescription("Specifies how to evaluate each segment of incoming text against the user-defined properties.")
    ->isRequired(true)
    ->withAllowableValues<std::string>(Matching::values())
    ->build());

const core::Property RouteText::TrimWhitespace(
    core::PropertyBuilder::createProperty("Ignore Leading/Trailing Whitespace")
    ->withDescription("Indicates whether or not the whitespace at the beginning and end should be ignored when evaluating a segment.")
    ->isRequired(true)
    ->withDefaultValue<bool>(true)
    ->build());

const core::Property RouteText::IgnoreCase(
    core::PropertyBuilder::createProperty("Ignore Case")
    ->withDescription("If true, capitalization will not be taken into account when comparing values. E.g., matching against 'HELLO' or 'hello' will have the same result. "
                      "This property is ignored if the 'Matching Strategy' is set to 'Satisfies Expression'.")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->build());

const core::Property RouteText::GroupingRegex(
    core::PropertyBuilder::createProperty("Grouping Regular Expression")
    ->withDescription("Specifies a Regular Expression to evaluate against each segment to determine which Group it should be placed in. "
                      "The Regular Expression must have at least one Capturing Group that defines the segment's Group. If multiple Capturing Groups exist in the Regular Expression, the values from all "
                      "Capturing Groups will be concatenated together. Two segments will not be placed into the same FlowFile unless they both have the same value for the Group "
                      "(or neither matches the Regular Expression). For example, to group together all lines in a CSV File by the first column, we can set this value to \"(.*?),.*\" (and use \"Per Line\" segmentation). "
                      "Two segments that have the same Group but different Relationships will never be placed into the same FlowFile.")
    ->build());

const core::Property RouteText::SegmentationStrategy(
    core::PropertyBuilder::createProperty("Segmentation Strategy")
    ->withDescription("Specifies what portions of the FlowFile content constitutes a single segment to be processed.")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(Segmentation::PER_LINE))
    ->withAllowableValues<std::string>(Segmentation::values())
    ->build());

const core::Relationship RouteText::Original("original", "The original input file will be routed to this destination when the segments have been successfully routed to 1 or more relationships");

const core::Relationship RouteText::Unmatched("unmatched", "Segments that do not satisfy the required user-defined rules will be routed to this Relationship");

const core::Relationship RouteText::Matched("matched", "Segments that satisfy the required user-defined rules will be routed to this Relationship");

RouteText::RouteText(const std::string& name, const utils::Identifier& uuid)
    : core::Processor(name, uuid), logger_(logging::LoggerFactory<RouteText>::getLogger()) {}

void RouteText::initialize() {
  setSupportedProperties({
     RoutingStrategy,
     MatchingStrategy,
     TrimWhitespace,
     IgnoreCase,
     GroupingRegex,
     SegmentationStrategy
  });
  setSupportedRelationships({Original, Unmatched, Matched});
}

static std::regex to_regex(const std::string& str) {
  return std::regex(str);
}

void RouteText::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*sessionFactory*/) {
  routing_ = utils::parseEnumProperty<Routing>(*context, RoutingStrategy);
  matching_ = utils::parseEnumProperty<Matching>(*context, MatchingStrategy);
  context->getProperty(TrimWhitespace.getName(), trim_);
  context->getProperty(IgnoreCase.getName(), ignore_case_);
  group_regex_ = context->getProperty(GroupingRegex) | utils::map(to_regex);
  segmentation_ = utils::parseEnumProperty<Segmentation>(*context, SegmentationStrategy);
}

class RouteText::ReadCallback : public InputStreamCallback {
  using Fn = std::function<void(Segment)>;
 public:
  explicit ReadCallback(Segmentation segmentation, Fn&& fn) : segmentation_(segmentation), fn_(std::move(fn)) {}

  int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
    std::vector<uint8_t> buffer;
    std::string_view content;
    if (auto opt_content = stream->tryGetBuffer()) {
      content = std::string_view{reinterpret_cast<const char*>(opt_content.value()), stream->size()};
    } else {
      // no O(1) content access, read it into our local buffer
      size_t total_read = 0;
      size_t remaining = stream->size();
      buffer.resize(remaining);
      while (remaining != 0) {
        size_t ret = stream->read(buffer.data() + total_read, remaining);
        if (io::isError(ret)) return -1;
        if (ret == 0) break;
        remaining -= ret;
        total_read += ret;
      }
      buffer.resize(total_read);
      content = std::string_view{reinterpret_cast<const char*>(buffer.data()), buffer.size()};
    }
    switch (segmentation_.value()) {
      case Segmentation::FULL_TEXT: {
        fn_({content, 0});
        return content.length();
      }
      case Segmentation::PER_LINE: {
        size_t segment_idx = 0;
        // do not strip \n\r characters before invocation to be
        // in-line with the nifi semantics
        std::string_view::size_type curr = 0;
        while (curr != std::string_view::npos) {
          // find beginning of next line
          auto next_line = content.find_first_not_of("\r\n", content.find_first_of("\r\n", curr));

          if (next_line == std::string_view::npos) {
            fn_({content.substr(curr), segment_idx});
          } else {
            fn_({content.substr(curr, next_line - curr), segment_idx});
          }
          curr = next_line;
          ++segment_idx;
        }
        return content.length();
      }
    }
    throw Exception(PROCESSOR_EXCEPTION, "Unknown segmentation strategy");
  }

 private:
  Segmentation segmentation_;
  Fn fn_;
};

class RouteText::MatchingContext {
 public:
  MatchingContext(core::ProcessContext& process_context, std::shared_ptr<core::FlowFile> flow_file, bool ignore_case)
    : process_context_(process_context),
      flow_file_(std::move(flow_file)),
      ignore_case_(ignore_case) {}

  const std::regex& getRegexProperty(const core::Property& prop) {
    auto it = regex_values_.find(prop.getName());
    if (it != regex_values_.end()) {
      return it->second;
    }
    std::string value;
    if (!process_context_.getDynamicProperty(prop, value, flow_file_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }
    std::regex::flag_type flags = std::regex::ECMAScript;
    if (ignore_case_) flags |= std::regex::icase;
    return (regex_values_[prop.getName()] = std::regex(value, flags));
  }

  const std::string& getStringProperty(const core::Property& prop) {
    auto it = string_values_.find(prop.getName());
    if (it != string_values_.end()) {
      return it->second;
    }
    std::string value;
    if (!process_context_.getDynamicProperty(prop, value, flow_file_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }
    return (string_values_[prop.getName()] = value);
  }

  core::ProcessContext& process_context_;
  std::shared_ptr<core::FlowFile> flow_file_;
  bool ignore_case_;

  std::map<std::string, std::string> string_values_;
  std::map<std::string, std::regex> regex_values_;
};

void RouteText::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  using GroupName = std::string;
  std::map<std::pair<core::Relationship, std::optional<GroupName>>, std::string> flow_file_contents;

  MatchingContext matching_context(*context, flow_file, ignore_case_);

  ReadCallback callback(segmentation_, [&] (Segment segment) {
    std::string_view original_value = segment.value_;
    if (segmentation_ == Segmentation::PER_LINE) {
      // do not consider the trailing \r\n characters in order to conform to nifi
      auto len = segment.value_.find_last_not_of("\r\n");
      if (len != std::string_view::npos) {
        ++len;
      }
      segment.value_ = segment.value_.substr(0, len);
    }
    if (trim_) {
      segment.value_ = utils::StringUtils::trim(segment.value_);
    }

    auto group = getGroup(segment.value_);
    switch (routing_.value()) {
      case Routing::ALL: {
        for (const auto& prop : dynamic_properties_) {
          if (!matchSegment(matching_context, segment, prop.second)) {
            flow_file_contents[{Unmatched, group}] += original_value;
            return;
          }
        }
        flow_file_contents[{Matched, group}] += original_value;
        return;
      }
      case Routing::ANY: {
        for (const auto& prop : dynamic_properties_) {
          if (matchSegment(matching_context, segment, prop.second)) {
            flow_file_contents[{Matched, group}] += original_value;
            return;
          }
        }
        flow_file_contents[{Unmatched, group}] += original_value;
        return;
      }
      case Routing::DYNAMIC: {
        bool routed = false;
        for (const auto& prop : dynamic_properties_) {
          if (matchSegment(matching_context, segment, prop.second)) {
            flow_file_contents[{dynamic_relationships_[prop.first], group}] += original_value;
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
  session->read(flow_file, &callback);

  for (const auto& flow_file_content : flow_file_contents) {
    auto new_flow_file = session->create(flow_file);
    new_flow_file->setAttribute(GROUP_ATTRIBUTE_NAME, flow_file_content.first.second)
    session->transfer(new_flow_file, flow_file_content.first.first);
  }

  session->transfer(flow_file, Original);
}

bool RouteText::matchSegment(MatchingContext& context, const Segment& segment, const core::Property& prop) const {
  switch (matching_.value()) {
    case Matching::EXPRESSION: {
      std::map<std::string, std::string> variables;
      variables["segment"] = segment.value_;
      variables["segmentNo"] = std::to_string(segment.idx_);
      if (segmentation_ == Segmentation::PER_LINE) {
        variables["line"] = segment.value_;
        variables["lineNo"] = std::to_string(segment.idx_);
      }
      std::string result;
      if (context.process_context_.getDynamicProperty(prop, result, context.flow_file_, variables)) {
        return utils::StringUtils::toBool(result).value_or(false);
      } else {
        throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
      }
    }
    case Matching::STARTS_WITH: {
      return utils::StringUtils::startsWith(segment.value_, context.getStringProperty(prop), !ignore_case_);
    }
    case Matching::ENDS_WITH: {
      return utils::StringUtils::endsWith(segment.value_, context.getStringProperty(prop), !ignore_case_);
    }
    case Matching::CONTAINS: {
      return utils::StringUtils::find(segment.value_, context.getStringProperty(prop), !ignore_case_) != std::string_view::npos;
    }
    case Matching::EQUALS: {
      return utils::StringUtils::equals(segment.value_, context.getStringProperty(prop), !ignore_case_);
    }
    case Matching::CONTAINS_REGEX: {
      std::cmatch match_result;
      return std::regex_search(segment.value_.begin(), segment.value_.end(), match_result, context.getRegexProperty(prop));
    }
    case Matching::MATCHES_REGEX: {
      std::cmatch match_result;
      return std::regex_match(segment.value_.begin(), segment.value_.end(), match_result, context.getRegexProperty(prop));
    }
  }
  throw Exception(PROCESSOR_EXCEPTION, "Unknown matching strategy");
}

std::optional<std::string> RouteText::getGroup(const std::string_view& segment) const {
  if (!group_regex_) {
    return std::nullopt;
  }
  std::cmatch match_result;
  if (!std::regex_match(segment.begin(), segment.end(), match_result, group_regex_.value())) {
    return std::nullopt;
  }
  bool first = true;
  std::string group_name;

  for (size_t idx = 1; idx < match_result.size(); ++idx) {
    if (!first) {
      group_name += ", ";
    }
    first = false;
    group_name += match_result[idx];
  }
  // TODO(adebreceni): warn if some capturing groups are not used
  return group_name;
}

void RouteText::onDynamicPropertyModified(const core::Property& /*orig_property*/, const core::Property& new_property) {
  dynamic_properties_[new_property.getName()] = new_property;

  std::set<core::Relationship> relationships{Original, Unmatched, Matched};

  for (const auto& prop : dynamic_properties_) {
    core::Relationship rel{prop.first, "Dynamic Route"};
    dynamic_relationships_[prop.first] = rel;
    relationships.insert(rel);
    logger_->log_info("RouteText registered dynamic route '%s' with expression '%s'", prop.first, prop.second.getValue().to_string());
  }

  setSupportedRelationships(relationships);
}

}  // org::apache::nifi::minifi::processors
