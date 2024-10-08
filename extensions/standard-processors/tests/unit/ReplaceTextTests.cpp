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

#include "GenerateFlowFile.h"
#include "LogAttribute.h"
#include "ReplaceText.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::processors {

struct ReplaceTextTestAccessor {
  ReplaceText processor_;
  ReplaceText::Parameters parameters_;

  ReplaceTextTestAccessor() : processor_{"replace_text"} {}

  void setEvaluationMode(EvaluationModeType evaluation_mode) { processor_.evaluation_mode_ = evaluation_mode; }
  void setReplacementStrategy(ReplacementStrategyType replacement_strategy) { processor_.replacement_strategy_ = replacement_strategy; }
  void setSearchValue(const std::string& search_value) { parameters_.search_value_ = search_value; }
  void setSearchRegex(const std::string& search_regex) { parameters_.search_regex_ = std::regex{search_regex}; }
  void setReplacementValue(const std::string& replacement_value) { parameters_.replacement_value_ = replacement_value; }

  std::string applyReplacements(const std::string& input, const std::shared_ptr<core::FlowFile>& flow_file = {}) const { return processor_.applyReplacements(input, flow_file, parameters_); }
};

}  // namespace org::apache::nifi::minifi::processors

TEST_CASE("ReplaceText can parse its properties", "[onSchedule]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setDebug<minifi::processors::ReplaceText>();

  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "One green bottle is hanging on the wall");
  plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::DataFormat, "Text");
  plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::UniqueFlowFiles, "false");

  std::shared_ptr<core::Processor> replace_text = plan->addProcessor("ReplaceText", "replace_text", minifi::processors::GenerateFlowFile::Success, true);
  plan->setProperty(replace_text, minifi::processors::ReplaceText::EvaluationMode, "Entire text");
  plan->setProperty(replace_text, minifi::processors::ReplaceText::LineByLineEvaluationMode, "Except-First-Line");
  plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, "Substitute Variables");
  plan->setProperty(replace_text, minifi::processors::ReplaceText::SearchValue, "apple");
  plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "orange");

  testController.runSession(plan);

  CHECK(LogTestController::getInstance().contains("the Evaluation Mode property is set to Entire text"));
  CHECK(LogTestController::getInstance().contains("the Line-by-Line Evaluation Mode property is set to Except-First-Line"));
  CHECK(LogTestController::getInstance().contains("the Replacement Strategy property is set to Substitute Variables"));
  CHECK(LogTestController::getInstance().contains("the Search Value property is set to apple"));
  CHECK(LogTestController::getInstance().contains("the Replacement Value property is set to orange"));
}

TEST_CASE("Prepend works correctly in ReplaceText", "[applyReplacements][Prepend]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::PREPEND);
  replace_text.setReplacementValue("orange");

  CHECK(replace_text.applyReplacements("") == "orange");
  CHECK(replace_text.applyReplacements("s and lemons") == "oranges and lemons");
}

TEST_CASE("Append works correctly in ReplaceText", "[applyReplacements][Append]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::APPEND);
  replace_text.setReplacementValue("orange");

  CHECK(replace_text.applyReplacements("") == "orange");
  CHECK(replace_text.applyReplacements("agent ") == "agent orange");
}

TEST_CASE("Regex Replace works correctly in ReplaceText", "[applyReplacements][Regex Replace]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::REGEX_REPLACE);
  replace_text.setSearchRegex("a\\w+e");
  replace_text.setReplacementValue("orange");

  CHECK(replace_text.applyReplacements("") == "");  // NOLINT(readability-container-size-empty)
  CHECK(replace_text.applyReplacements("apple tree") == "orange tree");
  CHECK(replace_text.applyReplacements("one apple, two apples") == "one orange, two oranges");
}

TEST_CASE("Regex Replace works with back references in ReplaceText", "[applyReplacements][Regex Replace]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::REGEX_REPLACE);
  replace_text.setSearchRegex("a(b+)c");
  replace_text.setReplacementValue("$& [found $1]");

  CHECK(replace_text.applyReplacements("") == "");  // NOLINT(readability-container-size-empty)
  CHECK(replace_text.applyReplacements("abc") == "abc [found b]");
  CHECK(replace_text.applyReplacements("cba") == "cba");
  CHECK(replace_text.applyReplacements("xxx abc yyy abbbc zzz") == "xxx abc [found b] yyy abbbc [found bbb] zzz");
}

TEST_CASE("Regex Replace treats non-existent back references as blank in ReplaceText", "[applyReplacements][Regex Replace]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::REGEX_REPLACE);
  replace_text.setSearchRegex("a(b+)c");
  replace_text.setReplacementValue("_$1_ '$2'");

  CHECK(replace_text.applyReplacements("") == "");  // NOLINT(readability-container-size-empty)
  CHECK(replace_text.applyReplacements("abc") == "_b_ ''");
  CHECK(replace_text.applyReplacements("cba") == "cba");
  CHECK(replace_text.applyReplacements("xxx abc yyy abbbc zzz") == "xxx _b_ '' yyy _bbb_ '' zzz");
}

TEST_CASE("Back references can be escaped when using Regex Replace in ReplaceText", "[applyReplacements][Regex Replace]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::REGEX_REPLACE);
  replace_text.setSearchRegex("a(b+)c");
  replace_text.setReplacementValue("$1 costs $$2");

  CHECK(replace_text.applyReplacements("") == "");  // NOLINT(readability-container-size-empty)
  CHECK(replace_text.applyReplacements("abc") == "b costs $2");
  CHECK(replace_text.applyReplacements("cba") == "cba");
  CHECK(replace_text.applyReplacements("xxx abc yyy abbbc zzz") == "xxx b costs $2 yyy bbb costs $2 zzz");
}

TEST_CASE("Literal replace works correctly in ReplaceText", "[applyReplacements][Literal Replace]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::LITERAL_REPLACE);
  replace_text.setSearchValue("apple");
  replace_text.setReplacementValue("orange");

  CHECK(replace_text.applyReplacements("") == "");  // NOLINT(readability-container-size-empty)
  CHECK(replace_text.applyReplacements("apple tree") == "orange tree");
  CHECK(replace_text.applyReplacements("one apple, two apples") == "one orange, two oranges");
}

TEST_CASE("Always Replace works correctly in ReplaceText", "[applyReplacements][Always Replace]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::ALWAYS_REPLACE);
  replace_text.setReplacementValue("orange");

  CHECK(replace_text.applyReplacements("") == "orange");
  CHECK(replace_text.applyReplacements("apple tree") == "orange");
  CHECK(replace_text.applyReplacements("one apple, two apples") == "orange");
}

TEST_CASE("Substitute Variables works correctly in ReplaceText", "[applyReplacements][Substitute Variables]") {
  minifi::processors::ReplaceTextTestAccessor replace_text;
  replace_text.setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  replace_text.setReplacementStrategy(minifi::processors::ReplacementStrategyType::SUBSTITUTE_VARIABLES);

  const auto flow_file = std::make_shared<minifi::FlowFileRecordImpl>();
  flow_file->setAttribute("color", "green");
  flow_file->setAttribute("food", "eggs and ham");

  CHECK(replace_text.applyReplacements("", flow_file) == "");  // NOLINT(readability-container-size-empty)
  CHECK(replace_text.applyReplacements("no placeholders", flow_file) == "no placeholders");
  CHECK(replace_text.applyReplacements("${color}", flow_file) == "green");
  CHECK(replace_text.applyReplacements("I like ${color} ${food}!", flow_file) == "I like green eggs and ham!");
  CHECK(replace_text.applyReplacements("it was ${color}er than ${color}", flow_file) == "it was greener than green");
  CHECK(replace_text.applyReplacements("an empty ${} is left alone", flow_file) == "an empty ${} is left alone");
  CHECK(replace_text.applyReplacements("not ${found} is left alone", flow_file) == "not ${found} is left alone");
  CHECK(replace_text.applyReplacements("this ${color} ${fruit} is sour", flow_file) == "this green ${fruit} is sour");
}

TEST_CASE("Regex Replace works correctly in ReplaceText in line by line mode", "[Line-by-Line][Regex Replace]") {
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
  plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\n pear\n orange\n banana\n");
  plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::DataFormat, "Text");
  plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::UniqueFlowFiles, "false");

  std::shared_ptr<core::Processor> replace_text = plan->addProcessor("ReplaceText", "replace_text", minifi::processors::GenerateFlowFile::Success, true);
  plan->setProperty(replace_text, minifi::processors::ReplaceText::EvaluationMode, magic_enum::enum_name(minifi::processors::EvaluationModeType::LINE_BY_LINE));
  plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::REGEX_REPLACE));
  plan->setProperty(replace_text, minifi::processors::ReplaceText::SearchValue, "[aeiou]");
  plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "_");

  std::string expected_output;
  SECTION("Replacing all lines") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::LineByLineEvaluationMode, magic_enum::enum_name(minifi::processors::LineByLineEvaluationModeType::ALL));
    expected_output = "_ppl_\n p__r\n _r_ng_\n b_n_n_\n";
  }
  SECTION("Replacing the first line") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::LineByLineEvaluationMode, magic_enum::enum_name(minifi::processors::LineByLineEvaluationModeType::FIRST_LINE));
    expected_output = "_ppl_\n pear\n orange\n banana\n";
  }
  SECTION("Replacing the last line") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::LineByLineEvaluationMode, magic_enum::enum_name(minifi::processors::LineByLineEvaluationModeType::LAST_LINE));
    expected_output = "apple\n pear\n orange\n b_n_n_\n";
  }
  SECTION("Replacing all lines except the first") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::LineByLineEvaluationMode, magic_enum::enum_name(minifi::processors::LineByLineEvaluationModeType::EXCEPT_FIRST_LINE));
    expected_output = "apple\n p__r\n _r_ng_\n b_n_n_\n";
  }
  SECTION("Replacing all lines except the last") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::LineByLineEvaluationMode, magic_enum::enum_name(minifi::processors::LineByLineEvaluationModeType::EXCEPT_LAST_LINE));
    expected_output = "_ppl_\n p__r\n _r_ng_\n banana\n";
  }
  SECTION("The output has fewer characters than the input") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "");
    expected_output = "ppl\n pr\n rng\n bnn\n";
  }
  SECTION("The output has more characters than the input") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "$&v$&");
    expected_output = "avappleve\n peveavar\n ovoravangeve\n bavanavanava\n";
  }
  SECTION("The start of line anchor works correctly") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::SearchValue, "^( ?)[aeiou]");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "$1_");
    expected_output = "_pple\n pear\n _range\n banana\n";
  }
  SECTION("The end of line anchor works correctly") {
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\n pear\n orange\n banana");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::SearchValue, "[aeiou]$");
    expected_output = "appl_\n pear\n orang_\n banan_";
  }
  SECTION("The end of line anchor works correctly with Windows line endings, too") {
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\r\n pear\r\n orange\r\n banana");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::SearchValue, "[aeiou]$");
    expected_output = "appl_\r\n pear\r\n orang_\r\n banan_";
  }
  SECTION("Prepend works correctly in line by line mode") {
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\npear\norange\nbanana\n");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::PREPEND));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "- ");
    expected_output = "- apple\n- pear\n- orange\n- banana\n";
  }
  SECTION("Append works correctly in line by line mode") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::APPEND));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, " tree");
    expected_output = "apple tree\n pear tree\n orange tree\n banana tree\n";
  }
  SECTION("Literal Replace works correctly in line by line mode") {
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::LITERAL_REPLACE));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::SearchValue, "a");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "*");
    expected_output = "*pple\n pe*r\n or*nge\n b*n*n*\n";
  }
  SECTION("Always Replace works correctly in line by line mode - without newline") {
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\n pear\n orange\n banana");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::ALWAYS_REPLACE));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "fruit");
    expected_output = "fruit\nfruit\nfruit\nfruit";
  }
  SECTION("Always Replace works correctly in line by line mode - with newline") {
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\n pear\n orange\n banana");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::ALWAYS_REPLACE));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "fruit\n");
    expected_output = "fruit\nfruit\nfruit\nfruit";
  }
  SECTION("Always Replace works correctly in line by line mode - with Windows line endings") {
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\r\n pear\r\n orange\r\n banana");
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::ALWAYS_REPLACE));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "fruit");
    expected_output = "fruit\r\nfruit\r\nfruit\r\nfruit";
  }

  std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attribute", minifi::processors::ReplaceText::Success, true);
  plan->setProperty(log_attribute, minifi::processors::LogAttribute::LogPayload, "true");

  testController.runSession(plan);

  CHECK(LogTestController::getInstance().contains(expected_output));
  LogTestController::getInstance().reset();
}

class HandleEmptyIncomingFlowFile {
 public:
  void setEvaluationMode(minifi::processors::EvaluationModeType evaluation_mode) { evaluation_mode_ = evaluation_mode; }
  void setReplacementStrategy(minifi::processors::ReplacementStrategyType replacement_strategy) { replacement_strategy_ = replacement_strategy; }
  void setExpectedOutput(const std::string& expected_output) { expected_output_ = expected_output; }

  void run() {
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

    std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::FileSize, "0 B");

    std::shared_ptr<core::Processor> replace_text = plan->addProcessor("ReplaceText", "replace_text", minifi::processors::GenerateFlowFile::Success, true);
    plan->setProperty(replace_text, minifi::processors::ReplaceText::EvaluationMode, magic_enum::enum_name(evaluation_mode_));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(replacement_strategy_));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, "hippopotamus");

    std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attribute", minifi::processors::ReplaceText::Success, true);
    plan->setProperty(log_attribute, minifi::processors::LogAttribute::LogPayload, "true");

    testController.runSession(plan);

    CHECK(LogTestController::getInstance().contains(expected_output_));
    LogTestController::getInstance().reset();
  }

 private:
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  minifi::processors::EvaluationModeType evaluation_mode_ = minifi::processors::EvaluationModeType::LINE_BY_LINE;
  minifi::processors::ReplacementStrategyType replacement_strategy_ = minifi::processors::ReplacementStrategyType::REGEX_REPLACE;
  std::string expected_output_;
};

TEST_CASE_METHOD(HandleEmptyIncomingFlowFile, "ReplaceText can prepend to an empty flow file in Entire text mode", "[Entire text][Prepend]") {
  setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  setReplacementStrategy(minifi::processors::ReplacementStrategyType::PREPEND);
  setExpectedOutput("Payload:\nhippopotamus\n");
  run();
}

TEST_CASE_METHOD(HandleEmptyIncomingFlowFile, "ReplaceText can append to an empty flow file in Entire text mode", "[Entire text][Append]") {
  setEvaluationMode(minifi::processors::EvaluationModeType::ENTIRE_TEXT);
  setReplacementStrategy(minifi::processors::ReplacementStrategyType::APPEND);
  setExpectedOutput("Payload:\nhippopotamus\n");
  run();
}

TEST_CASE_METHOD(HandleEmptyIncomingFlowFile, "ReplaceText can prepend to an empty flow file in Line-by-line mode", "[Line-by-Line][Prepend]") {
  setEvaluationMode(minifi::processors::EvaluationModeType::LINE_BY_LINE);
  setReplacementStrategy(minifi::processors::ReplacementStrategyType::PREPEND);
  setExpectedOutput("Size:0 Offset:0");
  run();
}

TEST_CASE_METHOD(HandleEmptyIncomingFlowFile, "ReplaceText can append to an empty flow file in Line-by-line mode", "[Line-by-line][Append]") {
  setEvaluationMode(minifi::processors::EvaluationModeType::LINE_BY_LINE);
  setReplacementStrategy(minifi::processors::ReplacementStrategyType::APPEND);
  setExpectedOutput("Size:0 Offset:0");
  run();
}

class UseExpressionLanguage {
 public:
  void setSearchValue(const std::string& search_value) { search_value_ = search_value; }
  void setReplacementValue(const std::string& replacement_value) { replacement_value_ = replacement_value; }
  void setExpectedOutput(const std::string& expected_output) { expected_output_ = expected_output; }

  void run() {
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

    std::shared_ptr<core::Processor> generate_flow_file = plan->addProcessor("GenerateFlowFile", "generate_flow_file");
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::CustomText, "apple\n pear\n orange\n banana\n");
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::DataFormat, "Text");
    plan->setProperty(generate_flow_file, minifi::processors::GenerateFlowFile::UniqueFlowFiles, "false");

    std::shared_ptr<core::Processor> update_attribute = plan->addProcessor("UpdateAttribute", "update_attribute", minifi::processors::GenerateFlowFile::Success, true);
    plan->setDynamicProperty(update_attribute, "substring", "an");
    plan->setDynamicProperty(update_attribute, "color", "blue");

    std::shared_ptr<core::Processor> replace_text = plan->addProcessor("ReplaceText", "replace_text", minifi::processors::GenerateFlowFile::Success, true);
    plan->setProperty(replace_text, minifi::processors::ReplaceText::EvaluationMode, magic_enum::enum_name(minifi::processors::EvaluationModeType::ENTIRE_TEXT));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementStrategy, magic_enum::enum_name(minifi::processors::ReplacementStrategyType::LITERAL_REPLACE));
    plan->setProperty(replace_text, minifi::processors::ReplaceText::SearchValue, search_value_);
    plan->setProperty(replace_text, minifi::processors::ReplaceText::ReplacementValue, replacement_value_);

    std::shared_ptr<core::Processor> log_attribute = plan->addProcessor("LogAttribute", "log_attribute", minifi::processors::ReplaceText::Success, true);
    plan->setProperty(log_attribute, minifi::processors::LogAttribute::LogPayload, "true");

    testController.runSession(plan);

    CHECK(LogTestController::getInstance().contains(expected_output_));
    LogTestController::getInstance().reset();
  }

 private:
  TestController testController;
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::string search_value_;
  std::string replacement_value_;
  std::string expected_output_;
};

TEST_CASE_METHOD(UseExpressionLanguage, "ReplaceText can use expression language in the Search Value", "[expression language][Search Value]") {
  setSearchValue("${substring}");
  setReplacementValue("*");
  setExpectedOutput("Payload:\napple\n pear\n or*ge\n b**a\n");
  run();
}

TEST_CASE_METHOD(UseExpressionLanguage, "ReplaceText can use expression language in the Replacement Value", "[expression language][Replacement Value]") {
  setSearchValue("orange");
  setReplacementValue("${color}berry");
  setExpectedOutput("Payload:\napple\n pear\n blueberry\n banana\n");
  run();
}

TEST_CASE_METHOD(UseExpressionLanguage, "ReplaceText can use expression language in both the Search and Replacement Values", "[expression language][Search Value][Replacement Value]") {
  setSearchValue("${substring}");
  setReplacementValue("${literal(2):plus(3)}");
  setExpectedOutput("Payload:\napple\n pear\n or5ge\n b55a\n");
  run();
}
