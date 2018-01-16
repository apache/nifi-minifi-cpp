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

#include <memory>
#include <string>

#include "../TestBase.h"
#include <ExtractText.h>
#include <GetFile.h>
#include <PutFile.h>
#include <LogAttribute.h>

namespace expression = org::apache::nifi::minifi::expression;

class MockFlowFile : public core::FlowFile {
  void releaseClaim(const std::shared_ptr<minifi::ResourceClaim> claim) override {}
};

TEST_CASE("Trivial static expression", "[expressionLanguageTestTrivialStaticExpr]") {  // NOLINT
  REQUIRE("a" == expression::make_static("a")({}));
}

TEST_CASE("Text expression", "[expressionLanguageTestTextExpression]") {  // NOLINT
  auto expr = expression::compile("text");
  REQUIRE("text" == expr({}));
}

TEST_CASE("Text expression with escaped dollar", "[expressionLanguageTestEscapedDollar]") {  // NOLINT
  auto expr = expression::compile("te$$xt");
  REQUIRE("te$xt" == expr({}));
}

TEST_CASE("Attribute expression", "[expressionLanguageTestAttributeExpression]") {  // NOLINT
  auto flow_file = std::make_shared<MockFlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${attr_a}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr({flow_file}));
}

TEST_CASE("Multi-attribute expression", "[expressionLanguageTestMultiAttributeExpression]") {  // NOLINT
  auto flow_file = std::make_shared<MockFlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  flow_file->addAttribute("attr_b", "__attr_value_b__");
  auto expr = expression::compile("text_before${attr_a}text_between${attr_b}text_after");
  REQUIRE("text_before__attr_value_a__text_between__attr_value_b__text_after" == expr({flow_file}));
}

TEST_CASE("Multi-flowfile attribute expression",
          "[expressionLanguageTestMultiFlowfileAttributeExpression]") {  // NOLINT
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr({flow_file_a}));

  auto flow_file_b = std::make_shared<MockFlowFile>();
  flow_file_b->addAttribute("attr_a", "__flow_b_attr_value_a__");
  REQUIRE("text_before__flow_b_attr_value_a__text_after" == expr({flow_file_b}));
}

TEST_CASE("Attribute expression with whitespace", "[expressionLanguageTestAttributeExpressionWhitespace]") {  // NOLINT
  auto flow_file = std::make_shared<MockFlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${\n\tattr_a \r}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr({flow_file}));
}

TEST_CASE("Special characters expression", "[expressionLanguageTestSpecialCharactersExpression]") {  // NOLINT
  auto expr = expression::compile("text_before|{}()[],:;\\/*#'\" \t\r\n${attr_a}}()text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before|{}()[],:;\\/*#'\" \t\r\n__flow_a_attr_value_a__}()text_after" == expr({flow_file_a}));
}

TEST_CASE("UTF-8 characters expression", "[expressionLanguageTestUTF8Expression]") {  // NOLINT
  auto expr = expression::compile("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹${attr_a}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__flow_a_attr_value_a__text_after" == expr({flow_file_a}));
}

TEST_CASE("UTF-8 characters attribute", "[expressionLanguageTestUTF8Attribute]") {  // NOLINT
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__");
  REQUIRE("text_before__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__text_after" == expr({flow_file_a}));
}

TEST_CASE("Single quoted attribute expression", "[expressionLanguageTestSingleQuotedAttributeExpression]") {  // NOLINT
  auto expr = expression::compile("text_before${'|{}()[],:;\\/*# \t\r\n$'}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr({flow_file_a}));
}

TEST_CASE("Double quoted attribute expression", "[expressionLanguageTestDoubleQuotedAttributeExpression]") {  // NOLINT
  auto expr = expression::compile("text_before${\"|{}()[],:;\\/*# \t\r\n$\"}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr({flow_file_a}));
}

TEST_CASE("Hostname function", "[expressionLanguageTestHostnameFunction]") {  // NOLINT
  auto expr = expression::compile("text_before${\n\t hostname ()\n\t }text_after");

  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);
  std::string expected("text_before");
  expected.append(hostname);
  expected.append("text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  REQUIRE(expected == expr({flow_file_a}));
}

TEST_CASE("ToUpper function", "[expressionLanguageTestToUpperFunction]") {  // NOLINT
  auto expr = expression::compile(R"(text_before${
                                       attr_a : toUpper()
                                     }text_after)");
  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr({flow_file_a}));
}

TEST_CASE("ToUpper function w/o whitespace", "[expressionLanguageTestToUpperFunctionWithoutWhitespace]") {  // NOLINT
  auto expr = expression::compile(R"(text_before${attr_a:toUpper()}text_after)");
  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr({flow_file_a}));
}

TEST_CASE("GetFile PutFile dynamic attribute", "[expressionLanguageTestGetFilePutFileDynamicAttribute]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::PutFile>();
  LogTestController::getInstance().setTrace<processors::ExtractText>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::PutFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  std::string in_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&in_dir[0]) != nullptr);

  std::string in_file(in_dir);
  in_file.append("/file");

  std::string out_dir("/tmp/gt.XXXXXX");
  REQUIRE(testController.createTempDirectory(&out_dir[0]) != nullptr);

  std::string out_file(out_dir);
  out_file.append("/extracted_attr/file");

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor(
      "GetFile",
      "GetFile");
  plan->setProperty(
      get_file,
      processors::GetFile::Directory.getName(), in_dir);
  plan->setProperty(
      get_file,
      processors::GetFile::KeepSourceFile.getName(),
      "false");
  plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "description"),
      true);
  auto extract_text = plan->addProcessor(
      "ExtractText",
      "ExtractText",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      extract_text,
      processors::ExtractText::Attribute.getName(), "extracted_attr_name");
  plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "description"),
      true);
  auto put_file = plan->addProcessor(
      "PutFile",
      "PutFile",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(
      put_file,
      processors::PutFile::Directory.getName(),
      out_dir + "/${extracted_attr_name}");
  plan->setProperty(
      put_file,
      processors::PutFile::ConflictResolution.getName(),
      processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);
  plan->setProperty(
      put_file,
      processors::PutFile::CreateDirs.getName(),
      "true");

  // Write test input
  {
    std::ofstream in_file_stream(in_file);
    in_file_stream << "extracted_attr";
  }

  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // ExtractText
  plan->runNextProcessor();  // Log
  plan->runNextProcessor();  // PutFile

  // Verify output
  {
    std::stringstream output_str;
    std::ifstream out_file_stream(out_file);
    output_str << out_file_stream.rdbuf();
    REQUIRE("extracted_attr" == output_str.str());
  }
}

TEST_CASE("Substring 2 arg", "[expressionLanguageSubstring2]") {  // NOLINT
  auto expr = expression::compile("text_before${attr:substring(6, 8)}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_text_after" == expr({flow_file_a}));
}

TEST_CASE("Substring 1 arg", "[expressionLanguageSubstring1]") {  // NOLINT
  auto expr = expression::compile("text_before${attr:substring(6)}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_value_a__text_after" == expr({flow_file_a}));
}

TEST_CASE("Substring Before", "[expressionLanguageSubstringBefore]") {  // NOLINT
  auto expr = expression::compile("${attr:substringBefore('attr_value_a__')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_" == expr({flow_file_a}));
}

TEST_CASE("Substring Before Last", "[expressionLanguageSubstringBeforeLast]") {  // NOLINT
  auto expr = expression::compile("${attr:substringBeforeLast('_a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_attr_value" == expr({flow_file_a}));
}

TEST_CASE("Substring After", "[expressionLanguageSubstringAfter]") {  // NOLINT
  auto expr = expression::compile("${attr:substringAfter('__flow_a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("_attr_value_a__" == expr({flow_file_a}));
}

TEST_CASE("Substring After Last", "[expressionLanguageSubstringAfterLast]") {  // NOLINT
  auto expr = expression::compile("${attr:substringAfterLast('_a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__" == expr({flow_file_a}));
}

TEST_CASE("Substring Before No Args", "[expressionLanguageSubstringBeforeNoArgs]") {  // NOLINT
  auto expr = expression::compile("${attr:substringBefore()}");
  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE_THROWS_WITH(expr({flow_file_a}), "Attempted to call incomplete function");
}

TEST_CASE("Substring After No Args", "[expressionLanguageSubstringAfterNoArgs]") {  // NOLINT
  auto expr = expression::compile("${attr:substringAfter()}");
  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE_THROWS_WITH(expr({flow_file_a}), "Attempted to call incomplete function");
}

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("Replace", "[expressionLanguageReplace]") {  // NOLINT
  auto expr = expression::compile("${attr:replace('.', '_')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename_txt" == expr({flow_file_a}));
}

TEST_CASE("Replace 2", "[expressionLanguageReplace2]") {  // NOLINT
  auto expr = expression::compile("${attr:replace(' ', '.')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a.brand.new.filename.txt" == expr({flow_file_a}));
}

TEST_CASE("Replace First", "[expressionLanguageReplaceFirst]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceFirst('a', 'the')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("the brand new filename.txt" == expr({flow_file_a}));
}

TEST_CASE("Replace First Regex", "[expressionLanguageReplaceFirstRegex]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceFirst('[br]', 'g')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a grand new filename.txt" == expr({flow_file_a}));
}

TEST_CASE("Replace All", "[expressionLanguageReplaceAll]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceAll('\\..*', '')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename" == expr({flow_file_a}));
}

TEST_CASE("Replace All 2", "[expressionLanguageReplaceAll2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceAll('a brand (new)', '$1')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("new filename.txt" == expr({flow_file_a}));
}

TEST_CASE("Replace All 3", "[expressionLanguageReplaceAll3]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceAll('XYZ', 'ZZZ')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr({flow_file_a}));
}

TEST_CASE("Replace Null", "[expressionLanguageReplaceNull]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr({flow_file_a}));
}

TEST_CASE("Replace Null 2", "[expressionLanguageReplaceNull2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr2", "a brand new filename.txt");
  REQUIRE("abc" == expr({flow_file_a}));
}

TEST_CASE("Replace Empty", "[expressionLanguageReplaceEmpty]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr({flow_file_a}));
}

TEST_CASE("Replace Empty 2", "[expressionLanguageReplaceEmpty2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "  \t  \r  \n  ");
  REQUIRE("abc" == expr({flow_file_a}));
}

TEST_CASE("Replace Empty 3", "[expressionLanguageReplaceEmpty2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr2", "test");
  REQUIRE("abc" == expr({flow_file_a}));
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX
