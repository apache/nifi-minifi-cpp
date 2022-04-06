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

#include <map>
#include <vector>
#include <utility>
#include <algorithm>
#include <set>

#if __cpp_lib_boyer_moore_searcher < 201603L
#include <experimental/functional>
template<typename It, typename Hash, typename Eq>
using boyer_moore_searcher = std::experimental::boyer_moore_searcher<It, Hash, Eq>;
#else
#include <functional>
template<typename It, typename Hash, typename Eq>
using boyer_moore_searcher = std::boyer_moore_searcher<It, Hash, Eq>;
#endif

#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "io/StreamPipe.h"
#include "logging/LoggerConfiguration.h"
#include "range/v3/view/transform.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/tail.hpp"
#include "range/v3/view/join.hpp"
#include "range/v3/view/cache1.hpp"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::processors {

const core::Property RouteText::RoutingStrategy(
    core::PropertyBuilder::createProperty("Routing Strategy")
    ->withDescription("Specifies how to determine which Relationship(s) to use when evaluating the segments "
                      "of incoming text against the 'Matching Strategy' and user-defined properties. "
                      "'Dynamic Routing' routes to all the matching dynamic relationships (or 'unmatched' if none matches). "
                      "'Route On All' routes to 'matched' iff all dynamic relationships match. "
                      "'Route On Any' routes to 'matched' iff any of the dynamic relationships match. ")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(Routing::DYNAMIC))
    ->withAllowableValues<std::string>(Routing::values())
    ->build());

const core::Property RouteText::MatchingStrategy(
    core::PropertyBuilder::createProperty("Matching Strategy")
    ->withDescription("Specifies how to evaluate each segment of incoming text against the user-defined properties. "
                      "Possible values are: 'Starts With', 'Ends With', 'Contains', 'Equals', 'Matches Regex', 'Contains Regex', 'Satisfies Expression'.")
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
                      "The Regular Expression must have at least one Capturing Group that defines the segment's Group. If multiple Capturing Groups "
                      "exist in the Regular Expression, the values from all Capturing Groups will be joined together with \", \". Two segments will not be "
                      "placed into the same FlowFile unless they both have the same value for the Group (or neither matches the Regular Expression). "
                      "For example, to group together all lines in a CSV File by the first column, we can set this value to \"(.*?),.*\" (and use \"Per Line\" segmentation). "
                      "Two segments that have the same Group but different Relationships will never be placed into the same FlowFile.")
    ->build());

const core::Property RouteText::GroupingFallbackValue(
    core::PropertyBuilder::createProperty("Grouping Fallback Value")
    ->withDescription("If the 'Grouping Regular Expression' is specified and the matching fails, this value will be considered the group of the segment.")
    ->withDefaultValue<std::string>("")
    ->build());

const core::Property RouteText::SegmentationStrategy(
    core::PropertyBuilder::createProperty("Segmentation Strategy")
    ->withDescription("Specifies what portions of the FlowFile content constitutes a single segment to be processed. "
                      "'Full Text' considers the whole content as a single segment, 'Per Line' considers each line of the content as a separate segment")
    ->isRequired(true)
    ->withDefaultValue<std::string>(toString(Segmentation::PER_LINE))
    ->withAllowableValues<std::string>(Segmentation::values())
    ->build());

const core::Relationship RouteText::Original("original", "The original input file will be routed to this destination");

const core::Relationship RouteText::Unmatched("unmatched", "Segments that do not satisfy the required user-defined rules will be routed to this Relationship");

const core::Relationship RouteText::Matched("matched", "Segments that satisfy the required user-defined rules will be routed to this Relationship");

RouteText::RouteText(const std::string& name, const utils::Identifier& uuid)
    : core::Processor(name, uuid), logger_(core::logging::LoggerFactory<RouteText>::getLogger()) {}

void RouteText::initialize() {
  setSupportedProperties({
     RoutingStrategy,
     MatchingStrategy,
     TrimWhitespace,
     IgnoreCase,
     GroupingRegex,
     GroupingFallbackValue,
     SegmentationStrategy
  });
  setSupportedRelationships({Original, Unmatched, Matched});
}

void RouteText::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*sessionFactory*/) {
  gsl_Expects(context);
  routing_ = utils::parseEnumProperty<Routing>(*context, RoutingStrategy);
  matching_ = utils::parseEnumProperty<Matching>(*context, MatchingStrategy);
  context->getProperty(TrimWhitespace.getName(), trim_);
  case_policy_ = context->getProperty<bool>(IgnoreCase).value_or(false) ? CasePolicy::IGNORE_CASE : CasePolicy::CASE_SENSITIVE;
  group_regex_ = context->getProperty(GroupingRegex) | utils::map([] (const auto& str) {return utils::Regex(str);});
  segmentation_ = utils::parseEnumProperty<Segmentation>(*context, SegmentationStrategy);
  context->getProperty(GroupingFallbackValue.getName(), group_fallback_);
}

class RouteText::ReadCallback : public InputStreamCallback {
  using Fn = std::function<void(Segment)>;

 public:
  ReadCallback(Segmentation segmentation, size_t file_size, Fn&& fn)
    : segmentation_(segmentation), file_size_(file_size), fn_(std::move(fn)) {}

  int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
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
    switch (segmentation_.value()) {
      case Segmentation::FULL_TEXT: {
        fn_({content, 0});
        return content.length();
      }
      case Segmentation::PER_LINE: {
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
        return content.length();
      }
    }
    throw Exception(PROCESSOR_EXCEPTION, "Unknown segmentation strategy");
  }

 private:
  Segmentation segmentation_;
  size_t file_size_;
  Fn fn_;
};

class RouteText::MatchingContext {
  struct CaseAwareHash {
    explicit CaseAwareHash(CasePolicy policy): policy_(policy) {}
    size_t operator()(char ch) const {
      if (policy_ == CasePolicy::CASE_SENSITIVE) {
        return static_cast<size_t>(ch);
      }
      return std::hash<int>{}(std::tolower(static_cast<unsigned char>(ch)));
    }

   private:
    CasePolicy policy_;
  };

  struct CaseAwareEq {
    explicit CaseAwareEq(CasePolicy policy): policy_(policy) {}
    bool operator()(char a, char b) const {
      if (policy_ == CasePolicy::CASE_SENSITIVE) {
        return a == b;
      }
      return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    }

   private:
    CasePolicy policy_;
  };
  using Searcher = boyer_moore_searcher<std::string::const_iterator, CaseAwareHash, CaseAwareEq>;

 public:
  MatchingContext(core::ProcessContext& process_context, std::shared_ptr<core::FlowFile> flow_file, CasePolicy case_policy)
    : process_context_(process_context),
      flow_file_(std::move(flow_file)),
      case_policy_(case_policy) {}

  const utils::Regex& getRegexProperty(const core::Property& prop) {
    auto it = regex_values_.find(prop.getName());
    if (it != regex_values_.end()) {
      return it->second;
    }
    std::string value;
    if (!process_context_.getDynamicProperty(prop, value, flow_file_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }
    std::vector<utils::Regex::Mode> flags;
    if (case_policy_ == CasePolicy::IGNORE_CASE) {
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
    if (!process_context_.getDynamicProperty(prop, value, flow_file_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }
    return (string_values_[prop.getName()] = value);
  }

  const Searcher& getSearcher(const core::Property& prop) {
    auto it = searcher_values_.find(prop.getName());
    if (it != searcher_values_.end()) {
      return it->second.searcher_;
    }
    std::string value;
    if (!process_context_.getDynamicProperty(prop, value, flow_file_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Missing dynamic property: '" + prop.getName() + "'");
    }

    return searcher_values_.emplace(
        std::piecewise_construct, std::forward_as_tuple(prop.getName()),
        std::forward_as_tuple(value, case_policy_)).first->second.searcher_;
  }

  core::ProcessContext& process_context_;
  std::shared_ptr<core::FlowFile> flow_file_;
  CasePolicy case_policy_;

  std::map<std::string, std::string> string_values_;
  std::map<std::string, utils::Regex> regex_values_;

  struct OwningSearcher {
    OwningSearcher(std::string str, CasePolicy case_policy)
      : str_(std::move(str)), searcher_(str_.cbegin(), str_.cend(), CaseAwareHash{case_policy}, CaseAwareEq{case_policy}) {}
    OwningSearcher(const OwningSearcher&) = delete;
    OwningSearcher(OwningSearcher&&) = delete;
    OwningSearcher& operator=(const OwningSearcher&) = delete;
    OwningSearcher& operator=(OwningSearcher&&) = delete;

    std::string str_;
    Searcher searcher_;
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

void RouteText::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  gsl_Expects(context && session);
  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  std::map<Route, std::string> flow_file_contents;

  MatchingContext matching_context(*context, flow_file, case_policy_);

  ReadCallback callback(segmentation_, flow_file->getSize(), [&] (Segment segment) {
    std::string_view original_value = segment.value_;
    std::string_view preprocessed_value = preprocess(segment.value_);

    if (matching_ != Matching::EXPRESSION) {
      // an Expression has access to the raw segment like in nifi
      // all others use the preprocessed_value
      segment.value_ = preprocessed_value;
    }

    // group extraction always uses the preprocessed
    auto group = getGroup(preprocessed_value);
    switch (routing_.value()) {
      case Routing::ALL: {
        if (std::all_of(dynamic_properties_.cbegin(), dynamic_properties_.cend(), [&] (const auto& prop) {
          return matchSegment(matching_context, segment, prop.second);
        })) {
          flow_file_contents[{Matched, group}] += original_value;
        } else {
          flow_file_contents[{Unmatched, group}] += original_value;
        }
        return;
      }
      case Routing::ANY: {
        if (std::any_of(dynamic_properties_.cbegin(), dynamic_properties_.cend(), [&] (const auto& prop) {
          return matchSegment(matching_context, segment, prop.second);
        })) {
          flow_file_contents[{Matched, group}] += original_value;
        } else {
          flow_file_contents[{Unmatched, group}] += original_value;
        }
        return;
      }
      case Routing::DYNAMIC: {
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
  session->read(flow_file, &callback);

  for (const auto& [route, content] : flow_file_contents) {
    auto new_flow_file = session->create(flow_file);
    if (route.group_name_) {
      new_flow_file->setAttribute(GROUP_ATTRIBUTE_NAME, route.group_name_.value());
    }
    session->writeBuffer(new_flow_file, content);
    session->transfer(new_flow_file, route.relationship_);
  }

  session->transfer(flow_file, Original);
}

std::string_view RouteText::preprocess(std::string_view str) const {
  if (segmentation_ == Segmentation::PER_LINE) {
    // do not consider the trailing \r\n characters in order to conform to nifi
    auto len = str.find_last_not_of("\r\n");
    if (len != std::string_view::npos) {
      str = str.substr(0, len + 1);
    } else {
      str = "";
    }
  }
  if (trim_) {
    str = utils::StringUtils::trim(str);
  }
  return str;
}

bool RouteText::matchSegment(MatchingContext& context, const Segment& segment, const core::Property& prop) const {
  switch (matching_.value()) {
    case Matching::EXPRESSION: {
      std::map<std::string, std::string> variables;
      variables["segment"] = segment.value_;
      variables["segmentNo"] = std::to_string(segment.idx_);
      if (segmentation_ == Segmentation::PER_LINE) {
        // for nifi compatibility
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
      return utils::StringUtils::startsWith(segment.value_, context.getStringProperty(prop), case_policy_ == CasePolicy::CASE_SENSITIVE);
    }
    case Matching::ENDS_WITH: {
      return utils::StringUtils::endsWith(segment.value_, context.getStringProperty(prop), case_policy_ == CasePolicy::CASE_SENSITIVE);
    }
    case Matching::CONTAINS: {
      return std::search(segment.value_.begin(), segment.value_.end(), context.getSearcher(prop)) != segment.value_.end();
    }
    case Matching::EQUALS: {
      return utils::StringUtils::equals(segment.value_, context.getStringProperty(prop), case_policy_ == CasePolicy::CASE_SENSITIVE);
    }
    case Matching::CONTAINS_REGEX: {
      std::string segment_str = std::string(segment.value_);
      return utils::regexSearch(segment_str, context.getRegexProperty(prop));
    }
    case Matching::MATCHES_REGEX: {
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
  // WARNING!! using a temporary std::string causes the omission of delimiters
  // in the output on Windows
  const std::string comma = ", ";
  // unused capturing groups default to empty string
  auto to_string = [] (const auto& submatch) -> std::string {return submatch;};
  return ranges::views::tail(match_result)  // only join the capture groups
    | ranges::views::transform(to_string)
    | ranges::views::cache1
    | ranges::views::join(comma)
    | ranges::to<std::string>();
}

void RouteText::onDynamicPropertyModified(const core::Property& /*orig_property*/, const core::Property& new_property) {
  dynamic_properties_[new_property.getName()] = new_property;

  std::set<core::Relationship> relationships{Original, Unmatched, Matched};

  for (const auto& [property_name, prop] : dynamic_properties_) {
    core::Relationship rel{property_name, "Dynamic Route"};
    dynamic_relationships_[property_name] = rel;
    relationships.insert(rel);
    logger_->log_info("RouteText registered dynamic route '%s' with expression '%s'", property_name, prop.getValue().to_string());
  }

  setSupportedRelationships(relationships);
}

REGISTER_RESOURCE(RouteText, "Routes textual data based on a set of user-defined rules. Each segment in an incoming FlowFile is "
                             "compared against the values specified by user-defined Properties. The mechanism by which the text is compared "
                             "to these user-defined properties is defined by the 'Matching Strategy'. The data is then routed according to "
                             "these rules, routing each segment of the text individually.");

}  // namespace org::apache::nifi::minifi::processors
