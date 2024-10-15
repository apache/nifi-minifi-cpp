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

#include "FlowFileRecord.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "processors/RouteText.h"

namespace processors = minifi::processors;

struct RouteTextController : public TestController {
  struct FlowFilePattern {
    FlowFilePattern& attr(const std::string& name, const std::optional<std::string>& value) {
      required_attributes_[name] = value;
      return *this;
    }
    FlowFilePattern& content(const std::string& content) {
      required_content_ = content;
      return *this;
    }

    std::map<std::string, std::optional<std::string>> required_attributes_;
    std::optional<std::string> required_content_;
  };

  struct FlowFilePatternVec : std::vector<FlowFilePattern> {
    FlowFilePatternVec() = default;
    FlowFilePatternVec(std::initializer_list<std::string> args) {
      for (const auto& arg : args) {
        push_back(FlowFilePattern().content(arg));
      }
    }
    FlowFilePatternVec(std::initializer_list<FlowFilePattern> patterns) {
      for (const auto& pattern : patterns) {
        push_back(pattern);
      }
    }
  };

  RouteTextController() {
    plan_ = createPlan();
    plan_->addProcessor(proc_, "RouteText");
    input_ = plan_->addConnection(nullptr, {"success", ""}, proc_);
    createOutput(processors::RouteText::Original);
    createOutput(processors::RouteText::Unmatched);
    createOutput(processors::RouteText::Matched);
  }

  void createOutput(const core::Relationship& rel) {
    outputs_[rel.getName()] = plan_->addConnection(proc_, rel, nullptr);
  }

  void verifyOutputRelationship(const std::string& rel_name, const FlowFilePatternVec& patterns) {
    size_t pattern_idx = 0;
    std::set<std::shared_ptr<core::FlowFile>> expired;
    while (auto flow_file = outputs_.at(rel_name)->poll(expired)) {
      REQUIRE(expired.empty());
      // more flowfiles than patterns
      REQUIRE(pattern_idx < patterns.size());
      const auto& pattern = patterns[pattern_idx++];
      for (const auto& attr : pattern.required_attributes_) {
        auto attr_value = flow_file->getAttribute(attr.first);
        REQUIRE(attr_value == attr.second);
      }
      if (pattern.required_content_) {
        REQUIRE(pattern.required_content_.value() == plan_->getContent(flow_file));
      }
    }
    // must use all patterns
    REQUIRE(pattern_idx == patterns.size());
  }

  void verifyAllOutput(const std::map<std::string, FlowFilePatternVec>& patterns) {
    FlowFilePatternVec all;
    for (const auto& [rel, files] : patterns) {
      for (const auto& file : files) {
        all.push_back(file);
      }
      verifyOutputRelationship(rel, files);
    }
    if (patterns.find("original") == patterns.end()) {
      // expectations on "original" were implicit
      verifyOutputRelationship("original", all);
    }
  }

  void run() const {
    while (!input_->isEmpty()) {
      plan_->runProcessor(proc_);
    }
  }

  void putFlowFile(const std::map<std::string, std::string>& attributes, const std::string& content) const {
    auto flow_file = std::make_shared<minifi::FlowFileRecordImpl>();
    for (const auto& attr : attributes) {
      flow_file->setAttribute(attr.first, attr.second);
    }
    auto content_session = plan_->getContentRepo()->createSession();
    auto claim = content_session->create();
    auto stream = content_session->write(claim);
    stream->write(reinterpret_cast<const uint8_t*>(content.c_str()), content.length());
    flow_file->setResourceClaim(claim);
    flow_file->setSize(stream->size());
    flow_file->setOffset(0);

    stream->close();
    content_session->commit();
    input_->put(flow_file);
  }

  std::shared_ptr<TestPlan> plan_ = createPlan();
  std::shared_ptr<core::Processor> proc_ = std::make_shared<processors::RouteText>("RouteText");
  std::map<std::string, minifi::Connection*> outputs_;
  minifi::Connection* input_;
};

TEST_CASE_METHOD(RouteTextController, "RouteText correctly handles Matching Strategies") {
  proc_->setProperty(processors::RouteText::RoutingStrategy, "Dynamic Routing");

  std::map<std::string, FlowFilePatternVec> expected{
      {"here", {}},
      {"matched", {}},
      {"unmatched", {}}
  };

  SECTION("Starts With") {
    proc_->setProperty(processors::RouteText::MatchingStrategy, "Starts With");
    proc_->setDynamicProperty("here", "se");
    SECTION("Case sensitive") {
      expected["here"] = {"seven"};
      expected["unmatched"] = {"Seven", "even"};
    }
    SECTION("Case insensitive") {
      proc_->setProperty(processors::RouteText::IgnoreCase, "true");
      expected["here"] = {"seven", "Seven"};
      expected["unmatched"] = {"even"};
    }
  }
  SECTION("Ends With") {
    proc_->setProperty(processors::RouteText::MatchingStrategy, "Ends With");
    proc_->setDynamicProperty("here", "ven");
    SECTION("Case sensitive") {
      expected["here"] = {"seven"};
      expected["unmatched"] = {"SeveN", "seten"};
    }
    SECTION("Case insensitive") {
      proc_->setProperty(processors::RouteText::IgnoreCase, "true");
      expected["here"] = {"seven", "SeveN"};
      expected["unmatched"] = {"seten"};
    }
  }
  SECTION("Contains") {
    proc_->setProperty(processors::RouteText::MatchingStrategy, "Contains");
    proc_->setDynamicProperty("here", "eve");
    SECTION("Case sensitive") {
      expected["here"] = {"seven"};
      expected["unmatched"] = {"SeVeN", "seren"};
    }
    SECTION("Case insensitive") {
      proc_->setProperty(processors::RouteText::IgnoreCase, "true");
      expected["here"] = {"seven", "SeVeN"};
      expected["unmatched"] = {"seren"};
    }
  }
  SECTION("Equals") {
    proc_->setProperty(processors::RouteText::MatchingStrategy, "Equals");
    proc_->setDynamicProperty("here", "seven");
    SECTION("Case sensitive") {
      expected["here"] = {"seven"};
      expected["unmatched"] = {"Seven", "seven1"};
    }
    SECTION("Case insensitive") {
      proc_->setProperty(processors::RouteText::IgnoreCase, "true");
      expected["here"] = {"seven", "Seven"};
      expected["unmatched"] = {"seven1"};
    }
  }
  SECTION("Matches Regex") {
    proc_->setProperty(processors::RouteText::MatchingStrategy, "Matches Regex");
    proc_->setDynamicProperty("here", "se.en");
    SECTION("Case sensitive") {
      expected["here"] = {"seven"};
      expected["unmatched"] = {"Seven", "sevena"};
    }
    SECTION("Case insensitive") {
      proc_->setProperty(processors::RouteText::IgnoreCase, "true");
      expected["here"] = {"seven", "Seven"};
      expected["unmatched"] = {"sevena"};
    }
  }
  SECTION("Contains Regex") {
    proc_->setProperty(processors::RouteText::MatchingStrategy, "Contains Regex");
    proc_->setDynamicProperty("here", ".ve");
    SECTION("Case sensitive") {
      expected["here"] = {"seven"};
      expected["unmatched"] = {"SeVeN", "ven"};
    }
    SECTION("Case insensitive") {
      proc_->setProperty(processors::RouteText::IgnoreCase, "true");
      expected["here"] = {"seven", "SeVeN"};
      expected["unmatched"] = {"ven"};
    }
  }
  SECTION("Satisfies Expression") {
    proc_->setProperty(processors::RouteText::MatchingStrategy, "Satisfies Expression");
    proc_->setDynamicProperty("here", "${segment:equals('seven')}");
    // case sensitivity does not matter here
    SECTION("Case sensitive") {
      expected["here"] = {"seven"};
      expected["unmatched"] = {"SeVeN", "ven"};
    }
    SECTION("Case insensitive matching does not apply in Satisfy Expression mode") {
      proc_->setProperty(processors::RouteText::IgnoreCase, "true");
      expected["here"] = {"seven"};
      expected["unmatched"] = {"SeVeN", "ven"};
    }
  }

  createOutput({"here", ""});

  for (const auto& route : expected) {
    for (const auto& ff : route.second) {
      putFlowFile({}, ff.required_content_.value());
    }
  }

  run();

  verifyAllOutput(expected);
}

TEST_CASE_METHOD(RouteTextController, "RouteText correctly handles Routing Strategies") {
  proc_->setProperty(processors::RouteText::MatchingStrategy, "Contains");
  proc_->setDynamicProperty("one", "apple");
  proc_->setDynamicProperty("two", "banana");

  createOutput({"one", ""});
  createOutput({"two", ""});

  std::map<std::string, FlowFilePatternVec> expected{
      {"one", {}},
      {"two", {}},
      {"matched", {}},
      {"unmatched", {}}
  };

  SECTION("Dynamic Routing") {
    proc_->setProperty(processors::RouteText::RoutingStrategy, "Dynamic Routing");

    expected["one"] = {"apple"};
    expected["two"] = {"banana"};
    expected["unmatched"] = {"other"};
  }
  SECTION("Route On All") {
    proc_->setProperty(processors::RouteText::RoutingStrategy, "Route On All");

    expected["matched"] = {"apple-banana"};
    expected["unmatched"] = {"apple", "none"};
  }
  SECTION("Route On Any") {
    proc_->setProperty(processors::RouteText::RoutingStrategy, "Route On Any");

    expected["matched"] = {"apple", "banana", "apple-banana"};
    expected["unmatched"] = {"none"};
  }

  for (const auto& route : expected) {
    for (const auto& ff : route.second) {
      putFlowFile({}, ff.required_content_.value());
    }
  }

  run();

  verifyAllOutput(expected);
}

TEST_CASE_METHOD(RouteTextController, "RouteText 'Per Line' segmentation") {
  proc_->setProperty(processors::RouteText::SegmentationStrategy, "Per Line");
  proc_->setProperty(processors::RouteText::MatchingStrategy, "Equals");
  proc_->setDynamicProperty("A", "A");
  proc_->setDynamicProperty("B", "B");

  createOutput({"A", ""});
  createOutput({"B", ""});

  std::string content = "A\nB\r\n A \r\n\r\rA";
  putFlowFile({}, content);

  std::map<std::string, FlowFilePatternVec> expected{
      {"A", {}},
      {"B", {}},
      {"matched", {}},
      {"unmatched", {}},
      {"original", {content}}
  };


  SECTION("Without trim") {
    proc_->setProperty(processors::RouteText::TrimWhitespace, "false");
    expected["A"] = {"A\n"};
    expected["B"] = {"B\r\n"};
    expected["unmatched"] = {" A \r\n\r\rA"};
  }

  SECTION("With trim") {
    proc_->setProperty(processors::RouteText::TrimWhitespace, "true");
    expected["A"] = {"A\n A \r\n\r\rA"};
    expected["B"] = {"B\r\n"};
  }

  run();

  verifyAllOutput(expected);
}

TEST_CASE_METHOD(RouteTextController, "RouteText 'Per Line' segmentation ignores trailing empty line") {
  proc_->setProperty(processors::RouteText::SegmentationStrategy, "Per Line");
  proc_->setProperty(processors::RouteText::RoutingStrategy, "Route On All");
  proc_->setProperty(processors::RouteText::MatchingStrategy, "Equals");
  proc_->setDynamicProperty("A", "A");

  std::string content;

  SECTION("Windows line ending") {
    content = "A\r\n";
  }
  SECTION("Simple line ending") {
    content = "A\n";
  }

  putFlowFile({}, content);

  std::map<std::string, FlowFilePatternVec> expected{
      {"matched", {content}},
      {"unmatched", {}},
      {"original", {content}}
  };

  run();

  verifyAllOutput(expected);
}

TEST_CASE_METHOD(RouteTextController, "RouteText can group segments") {
  proc_->setProperty(processors::RouteText::RoutingStrategy, "Dynamic Routing");
  proc_->setProperty(processors::RouteText::SegmentationStrategy, "Per Line");
  proc_->setProperty(processors::RouteText::MatchingStrategy, "Contains");
  proc_->setProperty(processors::RouteText::GroupingRegex, "group(.).*");
  proc_->setProperty(processors::RouteText::GroupingFallbackValue, "GROUPING_FAILURE :(");

  proc_->setDynamicProperty("A", "toA");
  proc_->setDynamicProperty("B", "toB");

  createOutput({"A", ""});
  createOutput({"B", ""});

  std::string content =
      "group1.toA(one)\ngroup1.toB(two)\ngroup1.toA(three)\ngroup2.toA(four)\n"
      "no_group.toA(five)\nno_group.toA(six)\ntoNone1\ngroup1.toNone2\ngroup1.toNone3";

  putFlowFile({}, content);

  std::map<std::string, FlowFilePatternVec> expected;

  expected["A"] = {
      FlowFilePattern{}.attr("RouteText.Group", "1").content("group1.toA(one)\ngroup1.toA(three)\n"),
      FlowFilePattern{}.attr("RouteText.Group", "2").content("group2.toA(four)\n"),
      FlowFilePattern{}.attr("RouteText.Group", "GROUPING_FAILURE :(").content("no_group.toA(five)\nno_group.toA(six)\n")
  };
  expected["B"] = {
      FlowFilePattern{}.attr("RouteText.Group", "1").content("group1.toB(two)\n")
  };
  expected["matched"] = FlowFilePatternVec{};
  expected["unmatched"] = {
      FlowFilePattern{}.attr("RouteText.Group", "1").content("group1.toNone2\ngroup1.toNone3"),
      FlowFilePattern{}.attr("RouteText.Group", "GROUPING_FAILURE :(").content("toNone1\n")
  };
  expected["original"] = {content};

  run();

  verifyAllOutput(expected);
}

TEST_CASE_METHOD(RouteTextController, "RouteText grouping uses empty strings for unused capture groups") {
  proc_->setProperty(processors::RouteText::RoutingStrategy, "Dynamic Routing");
  proc_->setProperty(processors::RouteText::SegmentationStrategy, "Per Line");
  proc_->setProperty(processors::RouteText::MatchingStrategy, "Contains");
  proc_->setProperty(processors::RouteText::GroupingRegex, "group(.)(\\..)?.*");

  proc_->setDynamicProperty("A", "toA");

  createOutput({"A", ""});

  std::string content =
      "group1.1:toA(one)\ngroup1.1:toA(two)\ngroup1.2:toA(three)\ngroup2:toA(four)\ngroup2:toA(five)";

  putFlowFile({}, content);

  std::map<std::string, FlowFilePatternVec> expected;

  expected["A"] = {
      FlowFilePattern{}.attr("RouteText.Group", "1, .1").content("group1.1:toA(one)\ngroup1.1:toA(two)\n"),
      FlowFilePattern{}.attr("RouteText.Group", "1, .2").content("group1.2:toA(three)\n"),
      FlowFilePattern{}.attr("RouteText.Group", "2, ").content("group2:toA(four)\ngroup2:toA(five)")
  };
  expected["matched"] = FlowFilePatternVec{};
  expected["unmatched"] = FlowFilePatternVec{};
  expected["original"] = {content};

  run();

  verifyAllOutput(expected);
}

TEST_CASE_METHOD(RouteTextController, "RouteText can match on Full Text") {
  proc_->setProperty(processors::RouteText::SegmentationStrategy, "Full Text");
  proc_->setProperty(processors::RouteText::MatchingStrategy, "Contains");

  proc_->setDynamicProperty("A", "toA");

  createOutput({"A", ""});

  std::string content = "toA\r\ntoA\ntoA\r";

  putFlowFile({}, content);

  std::map<std::string, FlowFilePatternVec> expected{
      {"matched", {}},
      {"unmatched", {}},
      {"A", {content}},
      {"original", {content}}
  };

  run();

  verifyAllOutput(expected);
}

TEST_CASE_METHOD(RouteTextController, "Expressions have access to injected variables") {
  proc_->setProperty(processors::RouteText::SegmentationStrategy, "Per Line");
  proc_->setProperty(processors::RouteText::MatchingStrategy, "Satisfies Expression");

  SECTION("Segment") {
    proc_->setDynamicProperty("A1", "${segment:startsWith('toA'):and(${segmentNo:equals('1')})}");
    proc_->setDynamicProperty("A2", "${segment:startsWith('toA'):and(${segmentNo:equals('2')})}");
  }
  SECTION("Line") {
    proc_->setDynamicProperty("A1", "${line:startsWith('toA'):and(${lineNo:equals('1')})}");
    proc_->setDynamicProperty("A2", "${line:startsWith('toA'):and(${lineNo:equals('2')})}");
  }

  createOutput({"A1", ""});
  createOutput({"A2", ""});

  std::string content = "toA one\ntoA two";

  putFlowFile({}, content);

  std::map<std::string, FlowFilePatternVec> expected{
      {"matched", {}},
      {"unmatched", {}},
      {"A1", {FlowFilePattern{}.attr("line", {}).attr("lineNo", {}).attr("segment", {}).attr("segmentNo", {}).content("toA one\n")}},
      {"A2", {FlowFilePattern{}.attr("line", {}).attr("lineNo", {}).attr("segment", {}).attr("segmentNo", {}).content("toA two")}},
      {"original", {content}}
  };

  run();

  verifyAllOutput(expected);
}
