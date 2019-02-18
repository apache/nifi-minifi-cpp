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

#include <time.h>

#include <memory>
#include <string>

#include "../TestBase.h"
#include <ExtractText.h>
#include <GetFile.h>
#include <PutFile.h>
#include <UpdateAttribute.h>
#include <LogAttribute.h>

namespace expression = org::apache::nifi::minifi::expression;

class MockFlowFile : public core::FlowFile {
  void releaseClaim(const std::shared_ptr<minifi::ResourceClaim> claim) override {}
};

TEST_CASE("Trivial static expression", "[expressionLanguageTestTrivialStaticExpr]") {  // NOLINT
  REQUIRE("a" == expression::make_static("a")({}).asString());
}

TEST_CASE("Text expression", "[expressionLanguageTestTextExpression]") {  // NOLINT
  auto expr = expression::compile("text");
  REQUIRE("text" == expr({}).asString());
}

TEST_CASE("Text expression with escaped dollar", "[expressionLanguageTestEscapedDollar]") {  // NOLINT
  auto expr = expression::compile("te$$xt");
  REQUIRE("te$xt" == expr({}).asString());
}

TEST_CASE("Attribute expression", "[expressionLanguageTestAttributeExpression]") {  // NOLINT
  auto flow_file = std::make_shared<MockFlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${attr_a}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr({flow_file}).asString());
}

TEST_CASE("Attribute expression (Null)", "[expressionLanguageTestAttributeExpressionNull]") {  // NOLINT
  auto expr = expression::compile("text_before${attr_a}text_after");
  std::shared_ptr<MockFlowFile> flow_file = nullptr;
  REQUIRE("text_beforetext_after" == expr({flow_file}).asString());
}

TEST_CASE("Multi-attribute expression", "[expressionLanguageTestMultiAttributeExpression]") {  // NOLINT
  auto flow_file = std::make_shared<MockFlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  flow_file->addAttribute("attr_b", "__attr_value_b__");
  auto expr = expression::compile("text_before${attr_a}text_between${attr_b}text_after");
  REQUIRE("text_before__attr_value_a__text_between__attr_value_b__text_after" == expr({flow_file}).asString());
}

TEST_CASE("Multi-flowfile attribute expression",
          "[expressionLanguageTestMultiFlowfileAttributeExpression]") {  // NOLINT
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr({flow_file_a}).asString());

  auto flow_file_b = std::make_shared<MockFlowFile>();
  flow_file_b->addAttribute("attr_a", "__flow_b_attr_value_a__");
  REQUIRE("text_before__flow_b_attr_value_a__text_after" == expr({flow_file_b}).asString());
}

TEST_CASE("Attribute expression with whitespace", "[expressionLanguageTestAttributeExpressionWhitespace]") {  // NOLINT
  auto flow_file = std::make_shared<MockFlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${\n\tattr_a \r}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr({flow_file}).asString());
}

TEST_CASE("Special characters expression", "[expressionLanguageTestSpecialCharactersExpression]") {  // NOLINT
  auto expr = expression::compile("text_before|{}()[],:;\\/*#'\" \t\r\n${attr_a}}()text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before|{}()[],:;\\/*#'\" \t\r\n__flow_a_attr_value_a__}()text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("UTF-8 characters expression", "[expressionLanguageTestUTF8Expression]") {  // NOLINT
  auto expr = expression::compile("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹${attr_a}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__flow_a_attr_value_a__text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("UTF-8 characters attribute", "[expressionLanguageTestUTF8Attribute]") {  // NOLINT
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__");
  REQUIRE("text_before__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("Single quoted attribute expression", "[expressionLanguageTestSingleQuotedAttributeExpression]") {  // NOLINT
  auto expr = expression::compile("text_before${'|{}()[],:;\\\\/*# \t\r\n$'}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("Double quoted attribute expression", "[expressionLanguageTestDoubleQuotedAttributeExpression]") {  // NOLINT
  auto expr = expression::compile("text_before${\"|{}()[],:;\\\\/*# \t\r\n$\"}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr({flow_file_a}).asString());
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
  REQUIRE(expected == expr({flow_file_a}).asString());
}

TEST_CASE("ToUpper function", "[expressionLanguageTestToUpperFunction]") {  // NOLINT
  auto expr = expression::compile(R"(text_before${
                                       attr_a : toUpper()
                                     }text_after)");
  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("ToUpper function w/o whitespace", "[expressionLanguageTestToUpperFunctionWithoutWhitespace]") {  // NOLINT
  auto expr = expression::compile(R"(text_before${attr_a:toUpper()}text_after)");
  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("ToLower function", "[expressionLanguageTestToLowerFunction]") {  // NOLINT
  auto expr = expression::compile(R"(text_before${
                                       attr_a : toLower()
                                     }text_after)");
  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr_a", "__FLOW_A_ATTR_VALUE_A__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("GetFile PutFile dynamic attribute", "[expressionLanguageTestGetFilePutFileDynamicAttribute]") {  // NOLINT
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::PutFile>();
  LogTestController::getInstance().setTrace<processors::ExtractText>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::PutFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  LogTestController::getInstance().setTrace<processors::UpdateAttribute>();

  auto conf = std::make_shared<minifi::Configure>();

  conf->set("nifi.my.own.property", "custom_value");

  auto plan = testController.createPlan(conf);
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
  auto update = plan->addProcessor(
      "UpdateAttribute",
      "UpdateAttribute",
      core::Relationship("success", "description"),
      true);
  update->setDynamicProperty("prop_attr", "${'nifi.my.own.property'}_added");
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
  plan->runNextProcessor();  // Update
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

  REQUIRE(LogTestController::getInstance().contains("key:prop_attr value:custom_value_added"));
}

TEST_CASE("Substring 2 arg", "[expressionLanguageSubstring2]") {  // NOLINT
  auto expr = expression::compile("text_before${attr:substring(6, 8)}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("Substring 1 arg", "[expressionLanguageSubstring1]") {  // NOLINT
  auto expr = expression::compile("text_before${attr:substring(6)}text_after");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_value_a__text_after" == expr({flow_file_a}).asString());
}

TEST_CASE("Substring Before", "[expressionLanguageSubstringBefore]") {  // NOLINT
  auto expr = expression::compile("${attr:substringBefore('attr_value_a__')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_" == expr({flow_file_a}).asString());
}

TEST_CASE("Substring Before Last", "[expressionLanguageSubstringBeforeLast]") {  // NOLINT
  auto expr = expression::compile("${attr:substringBeforeLast('_a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_attr_value" == expr({flow_file_a}).asString());
}

TEST_CASE("Substring After", "[expressionLanguageSubstringAfter]") {  // NOLINT
  auto expr = expression::compile("${attr:substringAfter('__flow_a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("_attr_value_a__" == expr({flow_file_a}).asString());
}

TEST_CASE("Substring After Last", "[expressionLanguageSubstringAfterLast]") {  // NOLINT
  auto expr = expression::compile("${attr:substringAfterLast('_a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__" == expr({flow_file_a}).asString());
}

TEST_CASE("Get Delimited", "[expressionLanguageGetDelimited]") {  // NOLINT
  auto expr = expression::compile("${attr:getDelimitedField(2)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE(" 32" == expr({flow_file_a}).asString());
}

TEST_CASE("Get Delimited 2", "[expressionLanguageGetDelimited2]") {  // NOLINT
  auto expr = expression::compile("${attr:getDelimitedField(1)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE("\"Jacobson, John\"" == expr({flow_file_a}).asString());
}

TEST_CASE("Get Delimited 3", "[expressionLanguageGetDelimited3]") {  // NOLINT
  auto expr = expression::compile("${attr:getDelimitedField(1, ',', '\\\"', '\\\\', 'true')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE("Jacobson, John" == expr({flow_file_a}).asString());
}

TEST_CASE("Starts With", "[expressionLanguageStartsWith]") {  // NOLINT
  auto expr = expression::compile("${attr:startsWith('a brand')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "A BRAND TEST");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Starts With 2", "[expressionLanguageStartsWith2]") {  // NOLINT
  auto expr = expression::compile("${attr:startsWith('a brand')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand TEST");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Ends With", "[expressionLanguageEndsWith]") {  // NOLINT
  auto expr = expression::compile("${attr:endsWith('txt')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.TXT");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Ends With 2", "[expressionLanguageEndsWith2]") {  // NOLINT
  auto expr = expression::compile("${attr:endsWith('TXT')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.TXT");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Contains", "[expressionLanguageContains]") {  // NOLINT
  auto expr = expression::compile("${attr:contains('new')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Contains 2", "[expressionLanguageContains2]") {  // NOLINT
  auto expr = expression::compile("${attr:contains('NEW')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("In", "[expressionLanguageIn]") {  // NOLINT
  auto expr = expression::compile("${attr:in('PAUL', 'JOHN', 'MIKE')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "JOHN");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("In 2", "[expressionLanguageIn2]") {  // NOLINT
  auto expr = expression::compile("${attr:in('RED', 'GREEN', 'BLUE')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "JOHN");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Substring Before No Args", "[expressionLanguageSubstringBeforeNoArgs]") {  // NOLINT
  REQUIRE_THROWS_WITH(expression::compile("${attr:substringBefore()}"),
                      "Expression language function substringBefore called with 1 argument(s), but 2 are required");
}

TEST_CASE("Substring After No Args", "[expressionLanguageSubstringAfterNoArgs]") {  // NOLINT
  REQUIRE_THROWS_WITH(expression::compile("${attr:substringAfter()}"); ,
                      "Expression language function substringAfter called with 1 argument(s), but 2 are required");
}

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("Replace", "[expressionLanguageReplace]") {  // NOLINT
  auto expr = expression::compile("${attr:replace('.', '_')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename_txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace 2", "[expressionLanguageReplace2]") {  // NOLINT
  auto expr = expression::compile("${attr:replace(' ', '.')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a.brand.new.filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace First", "[expressionLanguageReplaceFirst]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceFirst('a', 'the')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("the brand new filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace First Regex", "[expressionLanguageReplaceFirstRegex]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceFirst('[br]', 'g')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a grand new filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace All", "[expressionLanguageReplaceAll]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceAll('\\\\..*', '')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace All 2", "[expressionLanguageReplaceAll2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceAll('a brand (new)', '$1')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("new filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace All 3", "[expressionLanguageReplaceAll3]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceAll('XYZ', 'ZZZ')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace Null", "[expressionLanguageReplaceNull]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace Null 2", "[expressionLanguageReplaceNull2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr2", "a brand new filename.txt");
  REQUIRE("abc" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace Empty", "[expressionLanguageReplaceEmpty]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace Empty 2", "[expressionLanguageReplaceEmpty2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "  \t  \r  \n  ");
  REQUIRE("abc" == expr({flow_file_a}).asString());
}

TEST_CASE("Replace Empty 3", "[expressionLanguageReplaceEmpty2]") {  // NOLINT
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr2", "test");
  REQUIRE("abc" == expr({flow_file_a}).asString());
}

TEST_CASE("Matches", "[expressionLanguageMatches]") {  // NOLINT
  auto expr = expression::compile("${attr:matches('^(Ct|Bt|At):.*t$')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "At:est");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Matches 2", "[expressionLanguageMatches2]") {  // NOLINT
  auto expr = expression::compile("${attr:matches('^(Ct|Bt|At):.*t$')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "At:something");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Matches 3", "[expressionLanguageMatches3]") {  // NOLINT
  auto expr = expression::compile("${attr:matches('(Ct|Bt|At):.*t')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", " At:est");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Find", "[expressionLanguageFind]") {  // NOLINT
  auto expr = expression::compile("${attr:find('a [Bb]rand [Nn]ew')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Find 2", "[expressionLanguageFind2]") {  // NOLINT
  auto expr = expression::compile("${attr:find('Brand.*')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Find 3", "[expressionLanguageFind3]") {  // NOLINT
  auto expr = expression::compile("${attr:find('brand')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("IndexOf", "[expressionLanguageIndexOf]") {  // NOLINT
  auto expr = expression::compile("${attr:indexOf('a.*txt')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("-1" == expr({flow_file_a}).asString());
}

TEST_CASE("IndexOf2", "[expressionLanguageIndexOf2]") {  // NOLINT
  auto expr = expression::compile("${attr:indexOf('.')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("20" == expr({flow_file_a}).asString());
}

TEST_CASE("IndexOf3", "[expressionLanguageIndexOf3]") {  // NOLINT
  auto expr = expression::compile("${attr:indexOf('a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("0" == expr({flow_file_a}).asString());
}

TEST_CASE("IndexOf4", "[expressionLanguageIndexOf4]") {  // NOLINT
  auto expr = expression::compile("${attr:indexOf(' ')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("1" == expr({flow_file_a}).asString());
}

TEST_CASE("LastIndexOf", "[expressionLanguageLastIndexOf]") {  // NOLINT
  auto expr = expression::compile("${attr:lastIndexOf('a.*txt')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("-1" == expr({flow_file_a}).asString());
}

TEST_CASE("LastIndexOf2", "[expressionLanguageLastIndexOf2]") {  // NOLINT
  auto expr = expression::compile("${attr:lastIndexOf('.')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("20" == expr({flow_file_a}).asString());
}

TEST_CASE("LastIndexOf3", "[expressionLanguageLastIndexOf3]") {  // NOLINT
  auto expr = expression::compile("${attr:lastIndexOf('a')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("17" == expr({flow_file_a}).asString());
}

TEST_CASE("LastIndexOf4", "[expressionLanguageLastIndexOf4]") {  // NOLINT
  auto expr = expression::compile("${attr:lastIndexOf(' ')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("11" == expr({flow_file_a}).asString());
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("Plus Integer", "[expressionLanguagePlusInteger]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(13)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("24" == expr({flow_file_a}).asString());
}

TEST_CASE("Plus Decimal", "[expressionLanguagePlusDecimal]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(-13.34567)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-2.24567" == expr({flow_file_a}).asString());
}

TEST_CASE("Plus Exponent", "[expressionLanguagePlusExponent]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(10e+6)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("10000011" == expr({flow_file_a}).asString());
}

TEST_CASE("Plus Exponent 2", "[expressionLanguagePlusExponent2]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(10e+6)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11.345678901234");
  REQUIRE("10000011.345678901234351" == expr({flow_file_a}).asString());
}

TEST_CASE("Minus Integer", "[expressionLanguageMinusInteger]") {  // NOLINT
  auto expr = expression::compile("${attr:minus(13)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("-2" == expr({flow_file_a}).asString());
}

TEST_CASE("Minus Decimal", "[expressionLanguageMinusDecimal]") {  // NOLINT
  auto expr = expression::compile("${attr:minus(-13.34567)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("24.44567" == expr({flow_file_a}).asString());
}

TEST_CASE("Multiply Integer", "[expressionLanguageMultiplyInteger]") {  // NOLINT
  auto expr = expression::compile("${attr:multiply(13)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("143" == expr({flow_file_a}).asString());
}

TEST_CASE("Multiply Decimal", "[expressionLanguageMultiplyDecimal]") {  // NOLINT
  auto expr = expression::compile("${attr:multiply(-13.34567)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-148.136937" == expr({flow_file_a}).asString());
}

TEST_CASE("Divide Integer", "[expressionLanguageDivideInteger]") {  // NOLINT
  auto expr = expression::compile("${attr:divide(13)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("0.846153846153846" == expr({flow_file_a}).asString());
}

TEST_CASE("Divide Decimal", "[expressionLanguageDivideDecimal]") {  // NOLINT
  auto expr = expression::compile("${attr:divide(-13.34567)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-0.831730441409086" == expr({flow_file_a}).asString());
}

TEST_CASE("To Radix", "[expressionLanguageToRadix]") {  // NOLINT
  auto expr = expression::compile("${attr:toRadix(2,16)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "10");
  REQUIRE("0000000000001010" == expr({flow_file_a}).asString());
}

TEST_CASE("To Radix 2", "[expressionLanguageToRadix2]") {  // NOLINT
  auto expr = expression::compile("${attr:toRadix(16)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "13");
  REQUIRE("d" == expr({flow_file_a}).asString());
}

TEST_CASE("To Radix 3", "[expressionLanguageToRadix3]") {  // NOLINT
  auto expr = expression::compile("${attr:toRadix(23,8)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "-2347");
  REQUIRE("-000004a1" == expr({flow_file_a}).asString());
}

TEST_CASE("From Radix", "[expressionLanguageFromRadix]") {  // NOLINT
  auto expr = expression::compile("${attr:fromRadix(2)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "0000000000001010");
  REQUIRE("10" == expr({flow_file_a}).asString());
}

TEST_CASE("From Radix 2", "[expressionLanguageFromRadix2]") {  // NOLINT
  auto expr = expression::compile("${attr:fromRadix(16)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "d");
  REQUIRE("13" == expr({flow_file_a}).asString());
}

TEST_CASE("From Radix 3", "[expressionLanguageFromRadix3]") {  // NOLINT
  auto expr = expression::compile("${attr:fromRadix(23)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "-000004a1");
  REQUIRE("-2347" == expr({flow_file_a}).asString());
}

TEST_CASE("Random", "[expressionLanguageRandom]") {  // NOLINT
  auto expr = expression::compile("${random()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  auto result = expr({flow_file_a}).asSignedLong();
  REQUIRE(result > 0);
}

TEST_CASE("Chained call", "[expressionChainedCall]") {  // NOLINT
  auto expr = expression::compile("${attr:multiply(3):plus(1)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("22" == expr({flow_file_a}).asString());
}

TEST_CASE("Chained call 2", "[expressionChainedCall2]") {  // NOLINT
  auto expr = expression::compile("${literal(10):multiply(2):plus(1):multiply(2)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  REQUIRE(42 == expr({flow_file_a}).asSignedLong());
}

TEST_CASE("Chained call 3", "[expressionChainedCall3]") {  // NOLINT
  auto expr = expression::compile("${literal(10):multiply(2):plus(${attr:multiply(2)}):multiply(${attr})}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("238" == expr({flow_file_a}).asString());
}

TEST_CASE("LiteralBool", "[expressionLiteralBool]") {  // NOLINT
  auto expr = expression::compile("${literal(true)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE(true == expr({flow_file_a}).asBoolean());
}

TEST_CASE("LiteralBool 2", "[expressionLiteralBool2]") {  // NOLINT
  auto expr = expression::compile("${literal(false)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE(false == expr({flow_file_a}).asBoolean());
}

TEST_CASE("Is Null", "[expressionIsNull]") {  // NOLINT
  auto expr = expression::compile("${filename:isNull()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Is Null 2", "[expressionIsNull2]") {  // NOLINT
  auto expr = expression::compile("${filename:isNull()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "7");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Not Null", "[expressionNotNull]") {  // NOLINT
  auto expr = expression::compile("${filename:notNull()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Not Null 2", "[expressionNotNull2]") {  // NOLINT
  auto expr = expression::compile("${filename:notNull()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "7");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Is Empty", "[expressionIsEmpty]") {  // NOLINT
  auto expr = expression::compile("${filename:isEmpty()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Is Empty 2", "[expressionIsEmpty2]") {  // NOLINT
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Is Empty 3", "[expressionIsEmpty3]") {  // NOLINT
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", " \t\r\n ");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Is Empty 4", "[expressionIsEmpty4]") {  // NOLINT
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Is Empty 5", "[expressionIsEmpty5]") {  // NOLINT
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", " \t\r\n a \t\r\n ");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Equals", "[expressionEquals]") {  // NOLINT
  auto expr = expression::compile("${attr:equals('hello.txt')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "hello.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Equals 2", "[expressionEquals2]") {  // NOLINT
  auto expr = expression::compile("${attr:equals('hello.txt')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "helllo.txt");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Equals 3", "[expressionEquals3]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5):equals(6)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Equals Ignore Case", "[expressionEqualsIgnoreCase]") {  // NOLINT
  auto expr = expression::compile("${attr:equalsIgnoreCase('hElLo.txt')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "hello.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Equals Ignore Case 2", "[expressionEqualsIgnoreCase2]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5):equalsIgnoreCase(6)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("GT", "[expressionGT]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5):gt(5)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("GT2", "[expressionGT2]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):gt(6.05)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("GT3", "[expressionGT3]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):gt(6.15)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("GE", "[expressionGE]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5):ge(6)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("GE2", "[expressionGE2]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):ge(6.05)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("GE3", "[expressionGE3]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):ge(6.15)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("LT", "[expressionLT]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5):lt(5)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("LT2", "[expressionLT2]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):lt(6.05)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("LT3", "[expressionLT3]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):lt(6.15)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("LE", "[expressionLE]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5):le(6)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("LE2", "[expressionLE2]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):le(6.05)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("LE3", "[expressionLE3]") {  // NOLINT
  auto expr = expression::compile("${attr:plus(5.1):le(6.15)}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("And", "[expressionAnd]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('an')})}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("And 2", "[expressionAnd2]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('ab')})}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Or", "[expressionOr]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):or(${filename:substring(0, 2):equals('an')})}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Or 2", "[expressionOr2]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):or(${filename:substring(0, 2):equals('ab')})}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("Not", "[expressionNot]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('an')}):not()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("false" == expr({flow_file_a}).asString());
}

TEST_CASE("Not 2", "[expressionNot2]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('ab')}):not()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr({flow_file_a}).asString());
}

TEST_CASE("If Else", "[expressionIfElse]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename}):ifElse('yes', 'no')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("yes" == expr({flow_file_a}).asString());
}

TEST_CASE("If Else 2", "[expressionIfElse2]") {  // NOLINT
  auto expr = expression::compile("${filename:toLower():equals( ${filename}):ifElse('yes', 'no')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("filename", "An example file.txt");
  REQUIRE("no" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode JSON", "[expressionEncodeJSON]") {  // NOLINT
  auto expr = expression::compile("${message:escapeJson()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "This is a \"test!\"");
  REQUIRE("This is a \\\"test!\\\"" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode JSON", "[expressionDecodeJSON]") {  // NOLINT
  auto expr = expression::compile("${message:unescapeJson()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "This is a \\\"test!\\\"");
  REQUIRE("This is a \"test!\"" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode Decode JSON", "[expressionEncodeDecodeJSON]") {  // NOLINT
  auto expr = expression::compile("${message:escapeJson():unescapeJson()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "This is a \"test!\"");
  REQUIRE("This is a \"test!\"" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode XML", "[expressionEncodeXML]") {  // NOLINT
  auto expr = expression::compile("${message:escapeXml()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero &gt; One &lt; &quot;two!&quot; &amp; &apos;true&apos;" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode XML", "[expressionDecodeXML]") {  // NOLINT
  auto expr = expression::compile("${message:unescapeXml()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "Zero &gt; One &lt; &quot;two!&quot; &amp; &apos;true&apos;");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode Decode XML", "[expressionEncodeDecodeXML]") {  // NOLINT
  auto expr = expression::compile("${message:escapeXml():unescapeXml()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode HTML3", "[expressionEncodeHTML3]") {  // NOLINT
  auto expr = expression::compile("${message:escapeHtml3()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "¥ & < «");
  REQUIRE("&yen; &amp; &lt; &laquo;" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode HTML3", "[expressionDecodeHTML3]") {  // NOLINT
  auto expr = expression::compile("${message:unescapeHtml3()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &laquo;");
  REQUIRE("¥ & < «" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode Decode HTML3", "[expressionEncodeDecodeHTML3]") {  // NOLINT
  auto expr = expression::compile("${message:escapeHtml3():unescapeHtml3()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &laquo;");
  REQUIRE("&yen; &amp; &lt; &laquo;" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode HTML4", "[expressionEncodeHTML4]") {  // NOLINT
  auto expr = expression::compile("${message:escapeHtml4()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "¥ & Φ < «");
  REQUIRE("&yen; &amp; &Phi; &lt; &laquo;" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode HTML4", "[expressionDecodeHTML4]") {  // NOLINT
  auto expr = expression::compile("${message:unescapeHtml4()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "&yen; &iota; &amp; &lt; &laquo;");
  REQUIRE("¥ ι & < «" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode Decode HTML4", "[expressionEncodeDecodeHTML4]") {  // NOLINT
  auto expr = expression::compile("${message:escapeHtml4():unescapeHtml4()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &Pi; &laquo;");
  REQUIRE("&yen; &amp; &lt; &Pi; &laquo;" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode CSV", "[expressionEncodeCSV]") {  // NOLINT
  auto expr = expression::compile("${message:escapeCsv()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("\"Zero > One < \"\"two!\"\" & 'true'\"" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode CSV", "[expressionDecodeCSV]") {  // NOLINT
  auto expr = expression::compile("${message:unescapeCsv()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", R"("Zero > One < ""two!"" & 'true'")");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode CSV 2", "[expressionDecodeCSV2]") {  // NOLINT
  auto expr = expression::compile("${message:unescapeCsv()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", R"("quoted")");
  REQUIRE("\"quoted\"" == expr({flow_file_a}).asString());
}


TEST_CASE("Encode Decode CSV", "[expressionEncodeDecodeCSV]") {  // NOLINT
  auto expr = expression::compile("${message:escapeCsv():unescapeCsv()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr({flow_file_a}).asString());
}

#ifndef DISABLE_CURL
TEST_CASE("Encode URL", "[expressionEncodeURL]") {  // NOLINT
  auto expr = expression::compile("${message:urlEncode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE("some%20value%20with%20spaces" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode URL", "[expressionDecodeURL]") {  // NOLINT
  auto expr = expression::compile("${message:urlDecode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "some%20value%20with%20spaces");
  REQUIRE("some value with spaces" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode Decode URL", "[expressionEncodeDecodeURL]") {  // NOLINT
  auto expr = expression::compile("${message:urlEncode():urlDecode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE("some value with spaces" == expr({flow_file_a}).asString());
}
#else
TEST_CASE("Encode URL", "[expressionEncodeURLExcept]") {  // NOLINT
  auto expr = expression::compile("${message:urlEncode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE_THROWS(expr({flow_file_a}).asString());
}

TEST_CASE("Decode URL", "[expressionDecodeURLExcept]") {  // NOLINT
  auto expr = expression::compile("${message:urlDecode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "some%20value%20with%20spaces");
  REQUIRE_THROWS(expr({flow_file_a}).asString());
}

TEST_CASE("Encode Decode URL", "[expressionEncodeDecodeURLExcept]") {  // NOLINT
  auto expr = expression::compile("${message:urlEncode():urlDecode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE_THROWS(expr({flow_file_a}).asString());
}
#endif

#ifdef EXPRESSION_LANGUAGE_USE_DATE

TEST_CASE("Parse Date", "[expressionParseDate]") {  // NOLINT
  auto expr = expression::compile("${message:toDate('%Y/%m/%d', 'America/Los_Angeles')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "2014/04/30");
  REQUIRE("1398841200000" == expr({flow_file_a}).asString());
}

TEST_CASE("Format Date", "[expressionFormatDate]") {  // NOLINT
  auto expr = expression::compile("${message:format('%m-%d-%Y', 'GMT')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "1394755200000");
  REQUIRE("03-14-2014" == expr({flow_file_a}).asString());
}

TEST_CASE("Reformat Date", "[expressionReformatDate]") {  // NOLINT
  auto expr = expression::compile("${message:toDate('%Y/%m/%d', 'GMT'):format('%m-%d-%Y', 'America/New_York')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "2014/03/14");
  REQUIRE("03-13-2014" == expr({flow_file_a}).asString());
}

TEST_CASE("Now Date", "[expressionNowDate]") {  // NOLINT
  auto expr = expression::compile("${now():format('%Y')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "2014/03/14");
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);

  REQUIRE((lt.tm_year + 1900) == expr({flow_file_a}).asUnsignedLong());
}

#endif  // EXPRESSION_LANGUAGE_USE_DATE

TEST_CASE("IP", "[expressionIP]") {  // NOLINT
  auto expr = expression::compile("${ip()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  REQUIRE("" != expr({flow_file_a}).asString());
}

TEST_CASE("Full Hostname", "[expressionFullHostname]") {  // NOLINT
  auto expr = expression::compile("${hostname('true')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  REQUIRE("" != expr({flow_file_a}).asString());
}

TEST_CASE("UUID", "[expressionUuid]") {  // NOLINT
  auto expr = expression::compile("${UUID()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  REQUIRE(36 == expr({flow_file_a}).asString().length());
}

TEST_CASE("Trim", "[expressionTrim]") {  // NOLINT
  auto expr = expression::compile("${message:trim()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", " 1 2 3 ");
  REQUIRE("1 2 3" == expr({flow_file_a}).asString());
}

TEST_CASE("Append", "[expressionAppend]") {  // NOLINT
  auto expr = expression::compile("${message:append('.gz')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt.gz" == expr({flow_file_a}).asString());
}

TEST_CASE("Prepend", "[expressionPrepend]") {  // NOLINT
  auto expr = expression::compile("${message:prepend('a brand new ')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "filename.txt");
  REQUIRE("a brand new filename.txt" == expr({flow_file_a}).asString());
}

TEST_CASE("Length", "[expressionLength]") {  // NOLINT
  auto expr = expression::compile("${message:length()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "a brand new filename.txt");
  REQUIRE(24 == expr({flow_file_a}).asUnsignedLong());
}

TEST_CASE("Encode B64", "[expressionEncodeB64]") {  // NOLINT
  auto expr = expression::compile("${message:base64Encode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "admin:admin");
  REQUIRE("YWRtaW46YWRtaW4=" == expr({flow_file_a}).asString());
}

TEST_CASE("Decode B64", "[expressionDecodeB64]") {  // NOLINT
  auto expr = expression::compile("${message:base64Decode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "YWRtaW46YWRtaW4=");
  REQUIRE("admin:admin" == expr({flow_file_a}).asString());
}

TEST_CASE("Encode Decode B64", "[expressionEncodeDecodeB64]") {  // NOLINT
  auto expr = expression::compile("${message:base64Encode():base64Decode()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr({flow_file_a}).asString());
}

TEST_CASE("All Contains", "[expressionAllContains]") {  // NOLINT
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "hello 2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("All Contains 2", "[expressionAllContains2]") {  // NOLINT
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Contains", "[expressionAnyContains]") {  // NOLINT
  auto expr = expression::compile("${anyAttribute('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Contains 2", "[expressionAnyContains2]") {  // NOLINT
  auto expr = expression::compile("${anyAttribute('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("All Matching Contains", "[expressionAllMatchingContains]") {  // NOLINT
  auto expr = expression::compile("${allMatchingAttributes('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "hello 2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("All Matching Contains 2", "[expressionAllMatchingContains2]") {  // NOLINT
  auto expr = expression::compile("${allMatchingAttributes('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "hello 2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

TEST_CASE("All Matching Contains 3", "[expressionAllMatchingContains3]") {  // NOLINT
  auto expr = expression::compile("${allMatchingAttributes('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("abc_2", "hello 2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("All Matching Contains 4", "[expressionAllMatchingContains4]") {  // NOLINT
  auto expr = expression::compile("${allMatchingAttributes('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Matching Contains", "[expressionAnyMatchingContains]") {  // NOLINT
  auto expr = expression::compile("${anyMatchingAttribute('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Matching Contains 2", "[expressionAnyMatchingContains2]") {  // NOLINT
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Matching Contains 3", "[expressionAnyMatchingContains3]") {  // NOLINT
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("abc_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Matching Contains 4", "[expressionAnyMatchingContains4]") {  // NOLINT
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("xyz_1", "mello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("All Delineated Contains", "[expressionAllDelineatedContains]") {  // NOLINT
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,hello_2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("All Delineated Contains 2", "[expressionAllDelineatedContains2]") {  // NOLINT
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,mello_2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

TEST_CASE("All Delineated Contains 3", "[expressionAllDelineatedContains3]") {  // NOLINT
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \" \"):contains('1,h')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,hello_2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Delineated Contains", "[expressionAnyDelineatedContains]") {  // NOLINT
  auto expr = expression::compile("${anyDelineatedValue(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,mello_2");
  REQUIRE(expr({flow_file_a}).asBoolean());
}

TEST_CASE("Any Delineated Contains 2", "[expressionAnyDelineatedContains2]") {  // NOLINT
  auto expr = expression::compile("${anyDelineatedValue(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("word_list", "mello_1,mello_2");
  REQUIRE(!expr({flow_file_a}).asBoolean());
}

TEST_CASE("Count", "[expressionCount]") {  // NOLINT
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello'):count()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(1 == expr({flow_file_a}).asUnsignedLong());
}

TEST_CASE("Count 2", "[expressionCount2]") {  // NOLINT
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('mello'):count()}");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  flow_file_a->addAttribute("c", "hello 3");
  REQUIRE(2 == expr({flow_file_a}).asUnsignedLong());
}

TEST_CASE("Count 3", "[expressionCount3]") {  // NOLINT
  auto expr = expression::compile("abc${allAttributes('a', 'b'):contains('mello'):count()}xyz");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  flow_file_a->addAttribute("c", "hello 3");
  REQUIRE("abc2xyz" == expr({flow_file_a}).asString());
}

TEST_CASE("Join", "[expressionJoin]") {  // NOLINT
  auto expr = expression::compile("abc_${allAttributes('a', 'b'):prepend('def_'):append('_ghi'):join(\"|\")}_xyz");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "hello");
  flow_file_a->addAttribute("b", "mello");
  REQUIRE("abc_def_hello_ghi|def_mello_ghi_xyz" == expr({flow_file_a}).asString());
}

TEST_CASE("Join 2", "[expressionJoin2]") {  // NOLINT
  auto expr = expression::compile("abc_${allAttributes('a', 'b'):join(\"|\"):prepend('def_'):append('_ghi')}_xyz");

  auto flow_file_a = std::make_shared<MockFlowFile>();
  flow_file_a->addAttribute("a", "hello");
  flow_file_a->addAttribute("b", "mello");
  REQUIRE("abc_def_hello|mello_ghi_xyz" == expr({flow_file_a}).asString());
}
