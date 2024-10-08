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

#include <ctime>

#include <memory>
#include <string>
#ifdef WIN32
#ifdef _DEBUG
#pragma comment(lib, "libcurl-d.lib")
#else
#pragma comment(lib, "libcurl.lib")
#endif
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "crypt32.lib")
#endif
#include <curl/curl.h>
#include "impl/expression/Expression.h"
#include <ExtractText.h>
#include <GetFile.h>
#include <PutFile.h>
#include <UpdateAttribute.h>
#include "core/FlowFile.h"
#include <LogAttribute.h>
#include "utils/gsl.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "catch2/catch_approx.hpp"
#include "unit/ProvenanceTestHelper.h"
#include "date/tz.h"
#include "unit/TestUtils.h"

namespace expression = org::apache::nifi::minifi::expression;

TEST_CASE("Trivial static expression", "[expressionLanguageTestTrivialStaticExpr]") {
  REQUIRE("a" == expression::make_static("a")(expression::Parameters{ }).asString());
}

TEST_CASE("Text expression", "[expressionLanguageTestTextExpression]") {
  auto expr = expression::compile("text");
  REQUIRE("text" == expr(expression::Parameters{ }).asString());
}

TEST_CASE("Text expression with escaped dollar", "[expressionLanguageTestEscapedDollar]") {
  auto expr = expression::compile("te$$xt");
  REQUIRE("te$xt" == expr(expression::Parameters{ }).asString());
}

TEST_CASE("Attribute expression", "[expressionLanguageTestAttributeExpression]") {
  auto flow_file = std::make_shared<core::FlowFileImpl>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${attr_a}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr(expression::Parameters{ flow_file.get() }).asString());
}

TEST_CASE("Attribute expression (Null)", "[expressionLanguageTestAttributeExpressionNull]") {
  auto expr = expression::compile("text_before${attr_a}text_after");
  std::shared_ptr<core::FlowFile> flow_file = nullptr;
  REQUIRE("text_beforetext_after" == expr(expression::Parameters{ flow_file.get() }).asString());
}

TEST_CASE("Multi-attribute expression", "[expressionLanguageTestMultiAttributeExpression]") {
  auto flow_file = std::make_shared<core::FlowFileImpl>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  flow_file->addAttribute("attr_b", "__attr_value_b__");
  auto expr = expression::compile("text_before${attr_a}text_between${attr_b}text_after");
  REQUIRE("text_before__attr_value_a__text_between__attr_value_b__text_after" == expr(expression::Parameters{ flow_file.get() }).asString());
}

TEST_CASE("Multi-flowfile attribute expression",
    "[expressionLanguageTestMultiFlowfileAttributeExpression]") {
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());

  auto flow_file_b = std::make_shared<core::FlowFileImpl>();
  flow_file_b->addAttribute("attr_a", "__flow_b_attr_value_a__");
  REQUIRE("text_before__flow_b_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_b.get() }).asString());
}

TEST_CASE("Attribute expression with whitespace", "[expressionLanguageTestAttributeExpressionWhitespace]") {
  auto flow_file = std::make_shared<core::FlowFileImpl>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${\n\tattr_a \r}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr(expression::Parameters{ flow_file.get() }).asString());
}

TEST_CASE("Special characters expression", "[expressionLanguageTestSpecialCharactersExpression]") {
  auto expr = expression::compile("text_before|{}()[],:;\\/*#'\" \t\r\n${attr_a}}()text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before|{}()[],:;\\/*#'\" \t\r\n__flow_a_attr_value_a__}()text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("UTF-8 characters expression", "[expressionLanguageTestUTF8Expression]") {
  auto expr = expression::compile("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹${attr_a}text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("UTF-8 characters attribute", "[expressionLanguageTestUTF8Attribute]") {
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr_a", "__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__");
  REQUIRE("text_before__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Single quoted attribute expression", "[expressionLanguageTestSingleQuotedAttributeExpression]") {
  auto expr = expression::compile("text_before${'|{}()[],:;\\\\/*# \t\r\n$'}text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Double quoted attribute expression", "[expressionLanguageTestDoubleQuotedAttributeExpression]") {
  auto expr = expression::compile("text_before${\"|{}()[],:;\\\\/*# \t\r\n$\"}text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Hostname function", "[expressionLanguageTestHostnameFunction]") {
  auto expr = expression::compile("text_before${\n\t hostname ()\n\t }text_after");

  std::array<char, 1024> hostname{};
  gethostname(hostname.data(), 1023);
  std::string expected("text_before");
  expected.append(hostname.data());
  expected.append("text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  REQUIRE(expected == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("ToUpper function", "[expressionLanguageTestToUpperFunction]") {
  auto expr = expression::compile(R"(text_before${
                                       attr_a : toUpper()
                                     }text_after)");
  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("ToUpper function w/o whitespace", "[expressionLanguageTestToUpperFunctionWithoutWhitespace]") {
  auto expr = expression::compile(R"(text_before${attr_a:toUpper()}text_after)");
  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("ToLower function", "[expressionLanguageTestToLowerFunction]") {
  auto expr = expression::compile(R"(text_before${
                                       attr_a : toLower()
                                     }text_after)");
  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr_a", "__FLOW_A_ATTR_VALUE_A__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("GetFile PutFile dynamic attribute", "[expressionLanguageTestGetFilePutFileDynamicAttribute]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::PutFile>();
  LogTestController::getInstance().setTrace<minifi::processors::ExtractText>();
  LogTestController::getInstance().setTrace<minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<minifi::processors::PutFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<minifi::processors::UpdateAttribute>();

  auto conf = std::make_shared<minifi::ConfigureImpl>();
  conf->setHome(testController.createTempDirectory());

  conf->set("nifi.my.own.property", "custom_value");

  auto plan = testController.createPlan(conf);
  auto repo = std::make_shared<TestRepository>();

  auto in_dir = testController.createTempDirectory();
  REQUIRE(!in_dir.empty());

  auto in_file = in_dir / "file";
  auto out_dir = testController.createTempDirectory();
  REQUIRE(!out_dir.empty());

  auto out_file = out_dir / "extracted_attr" / "file";

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor("GetFile", "GetFile");
  plan->setProperty(get_file, minifi::processors::GetFile::Directory, in_dir.string());
  plan->setProperty(get_file, minifi::processors::GetFile::KeepSourceFile, "false");
  auto update = plan->addProcessor("UpdateAttribute", "UpdateAttribute", core::Relationship("success", "description"), true);
  update->setDynamicProperty("prop_attr", "${'nifi.my.own.property'}_added");
  plan->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
  auto extract_text = plan->addProcessor("ExtractText", "ExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(extract_text, minifi::processors::ExtractText::Attribute, "extracted_attr_name");
  plan->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
  auto put_file = plan->addProcessor("PutFile", "PutFile", core::Relationship("success", "description"), true);
  plan->setProperty(put_file, minifi::processors::PutFile::Directory, (out_dir / "${extracted_attr_name}").string());
  plan->setProperty(put_file, minifi::processors::PutFile::ConflictResolution, magic_enum::enum_name(minifi::processors::PutFile::FileExistsResolutionStrategy::replace));
  plan->setProperty(put_file, minifi::processors::PutFile::CreateDirs, "true");

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

TEST_CASE("Substring 2 arg", "[expressionLanguageSubstring2]") {
  auto expr = expression::compile("text_before${attr:substring(6, 8)}text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Substring 1 arg", "[expressionLanguageSubstring1]") {
  auto expr = expression::compile("text_before${attr:substring(6)}text_after");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Substring Before", "[expressionLanguageSubstringBefore]") {
  auto expr = expression::compile("${attr:substringBefore('attr_value_a__')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Substring Before Last", "[expressionLanguageSubstringBeforeLast]") {
  auto expr = expression::compile("${attr:substringBeforeLast('_a')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_attr_value" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Substring After", "[expressionLanguageSubstringAfter]") {
  auto expr = expression::compile("${attr:substringAfter('__flow_a')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("_attr_value_a__" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Substring After Last", "[expressionLanguageSubstringAfterLast]") {
  auto expr = expression::compile("${attr:substringAfterLast('_a')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Get Delimited", "[expressionLanguageGetDelimited]") {
  auto expr = expression::compile("${attr:getDelimitedField(2)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE(" 32" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Get Delimited 2", "[expressionLanguageGetDelimited2]") {
  auto expr = expression::compile("${attr:getDelimitedField(1)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE("\"Jacobson, John\"" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Get Delimited 3", "[expressionLanguageGetDelimited3]") {
  auto expr = expression::compile(R"(${attr:getDelimitedField(1, ',', '\"', '\\', 'true')})");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE("Jacobson, John" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Starts With", "[expressionLanguageStartsWith]") {
  auto expr = expression::compile("${attr:startsWith('a brand')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "A BRAND TEST");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Starts With 2", "[expressionLanguageStartsWith2]") {
  auto expr = expression::compile("${attr:startsWith('a brand')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand TEST");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Ends With", "[expressionLanguageEndsWith]") {
  auto expr = expression::compile("${attr:endsWith('txt')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.TXT");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Ends With 2", "[expressionLanguageEndsWith2]") {
  auto expr = expression::compile("${attr:endsWith('TXT')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.TXT");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Contains", "[expressionLanguageContains]") {
  auto expr = expression::compile("${attr:contains('new')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Contains 2", "[expressionLanguageContains2]") {
  auto expr = expression::compile("${attr:contains('NEW')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("In", "[expressionLanguageIn]") {
  auto expr = expression::compile("${attr:in('PAUL', 'JOHN', 'MIKE')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "JOHN");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("In 2", "[expressionLanguageIn2]") {
  auto expr = expression::compile("${attr:in('RED', 'GREEN', 'BLUE')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "JOHN");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Substring Before No Args", "[expressionLanguageSubstringBeforeNoArgs]") {
  REQUIRE_THROWS_WITH(expression::compile("${attr:substringBefore()}"), "Expression language function substringBefore called with 1 argument(s), but 2 are required");
}

TEST_CASE("Substring After No Args", "[expressionLanguageSubstringAfterNoArgs]") {
  REQUIRE_THROWS_WITH(expression::compile("${attr:substringAfter()}"), "Expression language function substringAfter called with 1 argument(s), but 2 are required");
}

TEST_CASE("Replace", "[expressionLanguageReplace]") {
  auto expr = expression::compile("${attr:replace('.', '_')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename_txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace 2", "[expressionLanguageReplace2]") {
  auto expr = expression::compile("${attr:replace(' ', '.')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a.brand.new.filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace First", "[expressionLanguageReplaceFirst]") {
  auto expr = expression::compile("${attr:replaceFirst('a', 'the')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("the brand new filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace First Regex", "[expressionLanguageReplaceFirstRegex]") {
  auto expr = expression::compile("${attr:replaceFirst('[br]', 'g')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a grand new filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace All", "[expressionLanguageReplaceAll]") {
  auto expr = expression::compile("${attr:replaceAll('\\\\..*', '')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace All 2", "[expressionLanguageReplaceAll2]") {
  auto expr = expression::compile("${attr:replaceAll('a brand (new)', '$1')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("new filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace All 3", "[expressionLanguageReplaceAll3]") {
  auto expr = expression::compile("${attr:replaceAll('XYZ', 'ZZZ')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace Null", "[expressionLanguageReplaceNull]") {
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace Null 2", "[expressionLanguageReplaceNull2]") {
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr2", "a brand new filename.txt");
  REQUIRE("abc" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace Empty", "[expressionLanguageReplaceEmpty]") {
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace Empty 2", "[expressionLanguageReplaceEmpty2]") {
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "  \t  \r  \n  ");
  REQUIRE("abc" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Replace Empty 3", "[expressionLanguageReplaceEmpty2]") {
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr2", "test");
  REQUIRE("abc" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Matches", "[expressionLanguageMatches]") {
  auto expr = expression::compile("${attr:matches('^(Ct|Bt|At):.*t$')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "At:est");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Matches 2", "[expressionLanguageMatches2]") {
  auto expr = expression::compile("${attr:matches('^(Ct|Bt|At):.*t$')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "At:something");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Matches 3", "[expressionLanguageMatches3]") {
  auto expr = expression::compile("${attr:matches('(Ct|Bt|At):.*t')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", " At:est");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Find", "[expressionLanguageFind]") {
  auto expr = expression::compile("${attr:find('a [Bb]rand [Nn]ew')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Find 2", "[expressionLanguageFind2]") {
  auto expr = expression::compile("${attr:find('Brand.*')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Find 3", "[expressionLanguageFind3]") {
  auto expr = expression::compile("${attr:find('brand')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("IndexOf", "[expressionLanguageIndexOf]") {
  auto expr = expression::compile("${attr:indexOf('a.*txt')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("-1" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("IndexOf2", "[expressionLanguageIndexOf2]") {
  auto expr = expression::compile("${attr:indexOf('.')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("20" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("IndexOf3", "[expressionLanguageIndexOf3]") {
  auto expr = expression::compile("${attr:indexOf('a')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("0" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("IndexOf4", "[expressionLanguageIndexOf4]") {
  auto expr = expression::compile("${attr:indexOf(' ')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("1" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LastIndexOf", "[expressionLanguageLastIndexOf]") {
  auto expr = expression::compile("${attr:lastIndexOf('a.*txt')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("-1" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LastIndexOf2", "[expressionLanguageLastIndexOf2]") {
  auto expr = expression::compile("${attr:lastIndexOf('.')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("20" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LastIndexOf3", "[expressionLanguageLastIndexOf3]") {
  auto expr = expression::compile("${attr:lastIndexOf('a')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("17" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LastIndexOf4", "[expressionLanguageLastIndexOf4]") {
  auto expr = expression::compile("${attr:lastIndexOf(' ')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("11" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Plus Integer", "[expressionLanguagePlusInteger]") {
  auto expr = expression::compile("${attr:plus(13)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("24" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Plus Decimal", "[expressionLanguagePlusDecimal]") {
  auto expr = expression::compile("${attr:plus(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-2.24567" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Plus Exponent", "[expressionLanguagePlusExponent]") {
  auto expr = expression::compile("${attr:plus(10e+6)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("10000011" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Plus Exponent 2", "[expressionLanguagePlusExponent2]") {
  auto expr = expression::compile("${attr:plus(10e+6)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11.345678901234");
  REQUIRE(10000011.345678901234 == Catch::Approx(expr(expression::Parameters{ flow_file_a.get() }).asLongDouble()));
}

TEST_CASE("Minus Integer", "[expressionLanguageMinusInteger]") {
  auto expr = expression::compile("${attr:minus(13)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("-2" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Minus Decimal", "[expressionLanguageMinusDecimal]") {
  auto expr = expression::compile("${attr:minus(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("24.44567" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Multiply Integer", "[expressionLanguageMultiplyInteger]") {
  auto expr = expression::compile("${attr:multiply(13)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("143" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Multiply Decimal", "[expressionLanguageMultiplyDecimal]") {
  auto expr = expression::compile("${attr:multiply(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE(-148.136937 == Catch::Approx(expr(expression::Parameters{ flow_file_a.get() }).asLongDouble()));
}

TEST_CASE("Divide Integer", "[expressionLanguageDivideInteger]") {
  auto expr = expression::compile("${attr:divide(13)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("0.846153846153846" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Divide Decimal", "[expressionLanguageDivideDecimal]") {
  auto expr = expression::compile("${attr:divide(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-0.831730441409086" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("To Radix", "[expressionLanguageToRadix]") {
  auto expr = expression::compile("${attr:toRadix(2,16)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "10");
  REQUIRE("0000000000001010" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("To Radix 2", "[expressionLanguageToRadix2]") {
  auto expr = expression::compile("${attr:toRadix(16)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "13");
  REQUIRE("d" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("To Radix 3", "[expressionLanguageToRadix3]") {
  auto expr = expression::compile("${attr:toRadix(23,8)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "-2347");
  REQUIRE("-000004a1" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("From Radix", "[expressionLanguageFromRadix]") {
  auto expr = expression::compile("${attr:fromRadix(2)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "0000000000001010");
  REQUIRE("10" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("From Radix 2", "[expressionLanguageFromRadix2]") {
  auto expr = expression::compile("${attr:fromRadix(16)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "d");
  REQUIRE("13" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("From Radix 3", "[expressionLanguageFromRadix3]") {
  auto expr = expression::compile("${attr:fromRadix(23)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "-000004a1");
  REQUIRE("-2347" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Random", "[expressionLanguageRandom]") {
  auto expr = expression::compile("${random()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  auto result = expr(expression::Parameters{ flow_file_a.get() }).asSignedLong();
  REQUIRE(result > 0);
}

TEST_CASE("Chained call", "[expressionChainedCall]") {
  auto expr = expression::compile("${attr:multiply(3):plus(1)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("22" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Chained call 2", "[expressionChainedCall2]") {
  auto expr = expression::compile("${literal(10):multiply(2):plus(1):multiply(2)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  REQUIRE(42 == expr(expression::Parameters{ flow_file_a.get() }).asSignedLong());
}

TEST_CASE("Chained call 3", "[expressionChainedCall3]") {
  auto expr = expression::compile("${literal(10):multiply(2):plus(${attr:multiply(2)}):multiply(${attr})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("238" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LiteralBool", "[expressionLiteralBool]") {
  auto expr = expression::compile("${literal(true)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE(true == expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("LiteralBool 2", "[expressionLiteralBool2]") {
  auto expr = expression::compile("${literal(false)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE(false == expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Is Null", "[expressionIsNull]") {
  auto expr = expression::compile("${filename:isNull()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Is Null 2", "[expressionIsNull2]") {
  auto expr = expression::compile("${filename:isNull()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "7");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Not Null", "[expressionNotNull]") {
  auto expr = expression::compile("${filename:notNull()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Not Null 2", "[expressionNotNull2]") {
  auto expr = expression::compile("${filename:notNull()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "7");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Is Empty", "[expressionIsEmpty]") {
  auto expr = expression::compile("${filename:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Is Empty 2", "[expressionIsEmpty2]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Is Empty 3", "[expressionIsEmpty3]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", " \t\r\n ");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Is Empty 4", "[expressionIsEmpty4]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Is Empty 5", "[expressionIsEmpty5]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", " \t\r\n a \t\r\n ");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Equals", "[expressionEquals]") {
  auto expr = expression::compile("${attr:equals('hello.txt')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "hello.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Equals 2", "[expressionEquals2]") {
  auto expr = expression::compile("${attr:equals('hello.txt')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "helllo.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Equals 3", "[expressionEquals3]") {
  auto expr = expression::compile("${attr:plus(5):equals(6)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Equals Ignore Case", "[expressionEqualsIgnoreCase]") {
  auto expr = expression::compile("${attr:equalsIgnoreCase('hElLo.txt')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "hello.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Equals Ignore Case 2", "[expressionEqualsIgnoreCase2]") {
  auto expr = expression::compile("${attr:plus(5):equalsIgnoreCase(6)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("GT", "[expressionGT]") {
  auto expr = expression::compile("${attr:plus(5):gt(5)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("GT2", "[expressionGT2]") {
  auto expr = expression::compile("${attr:plus(5.1):gt(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("GT3", "[expressionGT3]") {
  auto expr = expression::compile("${attr:plus(5.1):gt(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

// using :gt() to test string to integer parsing code
TEST_CASE("GT4 Value parsing errors", "[expressionGT4][outofrange]") {
  const char* test_str = nullptr;
  const char* expected_substr = nullptr;
  SECTION("integer out of range") {
    // 2 ^ 64, the smallest positive integer that's not representable even in uint64_t
    test_str = "18446744073709551616";
    expected_substr = "out of range";
  }
  SECTION("integer invalid") {
    test_str = "banana1337";
    expected_substr = "invalid argument";
  }
  SECTION("floating point out of range") {
    // largest IEEE754 quad precision float normal number times 10 (bumped the exponent by one)
    test_str = "1.1897314953572317650857593266280070162e+4933";
    expected_substr = "out of range";
  }
  SECTION("floating point invalid") {
    test_str = "app.le+1337";
    expected_substr = "invalid argument";
  }
  REQUIRE(test_str);
  REQUIRE(expected_substr);
  const auto expr = expression::compile("${attr1:gt(13.37)}");
  auto flow_file = std::make_shared<core::FlowFileImpl>();
  flow_file->addAttribute("attr1", test_str);
  try {
    const auto result = expr(expression::Parameters{flow_file.get()}).asString();
    REQUIRE(false);
  } catch (const std::exception& ex) {
    const std::string message = ex.what();
    // The exception message should be helpful enough to contain the problem description, and the problematic value
    CHECK(message.find(expected_substr) != std::string::npos);
    CHECK(message.find(test_str) != std::string::npos);
  }
}

TEST_CASE("GE", "[expressionGE]") {
  auto expr = expression::compile("${attr:plus(5):ge(6)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("GE2", "[expressionGE2]") {
  auto expr = expression::compile("${attr:plus(5.1):ge(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("GE3", "[expressionGE3]") {
  auto expr = expression::compile("${attr:plus(5.1):ge(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LT", "[expressionLT]") {
  auto expr = expression::compile("${attr:plus(5):lt(5)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LT2", "[expressionLT2]") {
  auto expr = expression::compile("${attr:plus(5.1):lt(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LT3", "[expressionLT3]") {
  auto expr = expression::compile("${attr:plus(5.1):lt(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LE", "[expressionLE]") {
  auto expr = expression::compile("${attr:plus(5):le(6)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LE2", "[expressionLE2]") {
  auto expr = expression::compile("${attr:plus(5.1):le(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("LE3", "[expressionLE3]") {
  auto expr = expression::compile("${attr:plus(5.1):le(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("And", "[expressionAnd]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('an')})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("And 2", "[expressionAnd2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('ab')})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Or", "[expressionOr]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):or(${filename:substring(0, 2):equals('an')})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Or 2", "[expressionOr2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):or(${filename:substring(0, 2):equals('ab')})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Not", "[expressionNot]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('an')}):not()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Not 2", "[expressionNot2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('ab')}):not()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("If Else", "[expressionIfElse]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename}):ifElse('yes', 'no')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("yes" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("If Else 2", "[expressionIfElse2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename}):ifElse('yes', 'no')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("filename", "An example file.txt");
  REQUIRE("no" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode JSON", "[expressionEncodeJSON]") {
  auto expr = expression::compile("${message:escapeJson()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "This is a \"test!\"");
  REQUIRE("This is a \\\"test!\\\"" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode JSON", "[expressionDecodeJSON]") {
  auto expr = expression::compile("${message:unescapeJson()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", R"(This is a \"test!\")");
  REQUIRE("This is a \"test!\"" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode Decode JSON", "[expressionEncodeDecodeJSON]") {
  auto expr = expression::compile("${message:escapeJson():unescapeJson()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "This is a \"test!\"");
  REQUIRE("This is a \"test!\"" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode XML", "[expressionEncodeXML]") {
  auto expr = expression::compile("${message:escapeXml()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero &gt; One &lt; &quot;two!&quot; &amp; &apos;true&apos;" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode XML", "[expressionDecodeXML]") {
  auto expr = expression::compile("${message:unescapeXml()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "Zero &gt; One &lt; &quot;two!&quot; &amp; &apos;true&apos;");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode Decode XML", "[expressionEncodeDecodeXML]") {
  auto expr = expression::compile("${message:escapeXml():unescapeXml()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode HTML3", "[expressionEncodeHTML3]") {
  auto expr = expression::compile("${message:escapeHtml3()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "¥ & < «");
  REQUIRE("&yen; &amp; &lt; &laquo;" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode HTML3", "[expressionDecodeHTML3]") {
  auto expr = expression::compile("${message:unescapeHtml3()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &laquo;");
  REQUIRE("¥ & < «" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode Decode HTML3", "[expressionEncodeDecodeHTML3]") {
  auto expr = expression::compile("${message:escapeHtml3():unescapeHtml3()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &laquo;");
  REQUIRE("&yen; &amp; &lt; &laquo;" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode HTML4", "[expressionEncodeHTML4]") {
  auto expr = expression::compile("${message:escapeHtml4()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "¥ & Φ < «");
  REQUIRE("&yen; &amp; &Phi; &lt; &laquo;" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode HTML4", "[expressionDecodeHTML4]") {
  auto expr = expression::compile("${message:unescapeHtml4()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "&yen; &iota; &amp; &lt; &laquo;");
  REQUIRE("¥ ι & < «" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode Decode HTML4", "[expressionEncodeDecodeHTML4]") {
  auto expr = expression::compile("${message:escapeHtml4():unescapeHtml4()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &Pi; &laquo;");
  REQUIRE("&yen; &amp; &lt; &Pi; &laquo;" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode CSV", "[expressionEncodeCSV]") {
  auto expr = expression::compile("${message:escapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("\"Zero > One < \"\"two!\"\" & 'true'\"" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode CSV", "[expressionDecodeCSV]") {
  auto expr = expression::compile("${message:unescapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", R"("Zero > One < ""two!"" & 'true'")");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode CSV 2", "[expressionDecodeCSV2]") {
  auto expr = expression::compile("${message:unescapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", R"("quoted")");
  REQUIRE("\"quoted\"" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode Decode CSV", "[expressionEncodeDecodeCSV]") {
  auto expr = expression::compile("${message:escapeCsv():unescapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode URL", "[expressionEncodeURL]") {
  auto expr = expression::compile("${message:urlEncode()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE("some%20value%20with%20spaces" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode URL", "[expressionDecodeURL]") {
  auto expr = expression::compile("${message:urlDecode()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "some%20value%20with%20spaces");
  REQUIRE("some value with spaces" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode Decode URL", "[expressionEncodeDecodeURL]") {
  auto expr = expression::compile("${message:urlEncode():urlDecode()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE("some value with spaces" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Parse Date", "[expressionParseDate]") {
#ifdef WIN32
  expression::dateSetInstall(TZ_DATA_DIR);
#endif
  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "2014/04/30");
  CHECK("1398841200000" == expression::compile("${message:toDate('%Y/%m/%d', 'America/Los_Angeles')}")(expression::Parameters{ flow_file_a.get() }).asString());

  flow_file_a->addAttribute("trillion_utc", "2001/09/09 01:46:40.000Z");
  flow_file_a->addAttribute("trillion_paris", "2001/09/09 03:46:40.000Z");
  flow_file_a->addAttribute("trillion_la", "2001/09/08 18:46:40.000Z");
  CHECK("1000000000000" == expression::compile("${trillion_utc:toDate('%Y/%m/%d %H:%M:%SZ', 'UTC')}")(expression::Parameters{ flow_file_a.get() }).asString());
  CHECK("1000000000000" == expression::compile("${trillion_paris:toDate('%Y/%m/%d %H:%M:%SZ', 'Europe/Paris')}")(expression::Parameters{ flow_file_a.get() }).asString());
  CHECK("1000000000000" == expression::compile("${trillion_la:toDate('%Y/%m/%d %H:%M:%SZ', 'America/Los_Angeles')}")(expression::Parameters{ flow_file_a.get() }).asString());

  flow_file_a->addAttribute("timestamp_with_zone_info_00_00", "2023-03-02T03:49:55.190+08:45");
  flow_file_a->addAttribute("timestamp_with_zone_info_08_45", "2023-03-02T03:49:55.190+08:45");

  CHECK("1677697495190" == expression::compile("${timestamp_with_zone_info_00_00:toDate('%FT%T%Ez', 'UTC')}")(expression::Parameters{ flow_file_a.get() }).asString());
  CHECK("1677697495190" == expression::compile("${timestamp_with_zone_info_08_45:toDate('%FT%T%Ez', 'UTC')}")(expression::Parameters{ flow_file_a.get() }).asString());

  flow_file_a->addAttribute("invalid_timestamp_1", " 2023-03-02T03:49:55.190+08:45");
  flow_file_a->addAttribute("invalid_timestamp_2", "2023-03-02T03:49:55.190+08:45 ");
  flow_file_a->addAttribute("invalid_timestamp_3", "2023-03-02 03:49:55.190+08:45 ");

  REQUIRE_THROWS_AS(expression::compile("${invalid_timestamp_1:toDate('%FT%T%Ez', 'UTC')}")(expression::Parameters{ flow_file_a.get() }), std::runtime_error);
  REQUIRE_THROWS_AS(expression::compile("${invalid_timestamp_2:toDate('%FT%T%Ez', 'UTC')}")(expression::Parameters{ flow_file_a.get() }), std::runtime_error);
  REQUIRE_THROWS_AS(expression::compile("${invalid_timestamp_3:toDate('%FT%T%Ez', 'UTC')}")(expression::Parameters{ flow_file_a.get() }), std::runtime_error);
}

TEST_CASE("Reformat Date", "[expressionReformatDate]") {
#ifdef WIN32
  expression::dateSetInstall(TZ_DATA_DIR);
#endif
  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "2014/03/14");
  flow_file_a->addAttribute("blue", "20130917162643");

  CHECK("03-13-2014" == expression::compile("${message:toDate('%Y/%m/%d', 'UTC'):format('%m-%d-%Y', 'America/New_York')}")(expression::Parameters{ flow_file_a.get() }).asString());

  auto blue_utc_expr = expression::compile("${blue:toDate('%Y%m%d%H%M%S', 'UTC'):format('%Y/%m/%d %H:%M:%SZ', 'UTC')}");
  auto blue_paris_expr = expression::compile("${blue:toDate('%Y%m%d%H%M%S', 'UTC'):format('%Y/%m/%d %H:%M:%SZ', 'Europe/Paris')}");
  auto blue_la_expr = expression::compile("${blue:toDate('%Y%m%d%H%M%S', 'UTC'):format('%Y/%m/%d %H:%M:%SZ', 'America/Los_Angeles')}");
  CHECK("2013/09/17 16:26:43.000Z" == blue_utc_expr(expression::Parameters{ flow_file_a.get() }).asString());
  CHECK("2013/09/17 18:26:43.000Z" == blue_paris_expr(expression::Parameters{ flow_file_a.get() }).asString());
  CHECK("2013/09/17 09:26:43.000Z" == blue_la_expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Now Date", "[expressionNowDate]") {
#ifdef WIN32
  expression::dateSetInstall(TZ_DATA_DIR);
#endif
  auto expr = expression::compile("${now():format('%Y')}");
  auto current_year = date::year_month_day{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())}.year().operator int();

  CHECK(current_year == expr(expression::Parameters{ }).asSignedLong());
}

TEST_CASE("Parse RFC3339 with Expression Language toDate") {
  using date::sys_days;
  using org::apache::nifi::minifi::utils::timeutils::parseRfc3339;
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  using std::chrono::milliseconds;

  milliseconds expected_second = std::chrono::floor<milliseconds>((sys_days(2023_y / 03 / 01) + 19h + 04min + 55s).time_since_epoch());
  milliseconds expected_tenth_second = std::chrono::floor<milliseconds>((sys_days(2023_y / 03 / 01) + 19h + 04min + 55s + 100ms).time_since_epoch());
  milliseconds expected_milli_second = std::chrono::floor<milliseconds>((sys_days(2023_y / 03 / 01) + 19h + 04min + 55s + 190ms).time_since_epoch());

  CHECK(expression::compile("${literal('2023-03-01T19:04:55Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55.1Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_tenth_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55.19Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55.190Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55.190999Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01t19:04:55z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01t19:04:55.190z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01T20:04:55+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01T20:04:55.190+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01T20:04:55.190999+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 20:04:55+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01 20:04:55.1+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_tenth_second.count());
  CHECK(expression::compile("${literal('2023-03-01 20:04:55.19+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 20:04:55.190+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 20:04:55.190999+01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01_19:04:55Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01_19:04:55z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55.1Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_tenth_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55.19Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55.190Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01_19:04:55.190Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55.190999Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01_19:04:55.190999Z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55.190z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01_19:04:55.190z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55.190999z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01_19:04:55.190999z'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55-00:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01 19:04:55.190-00:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55-00:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55.190-00:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-02T03:49:55+08:45'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55+00:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());
  CHECK(expression::compile("${literal('2023-03-01T19:04:55.190+00:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_milli_second.count());
  CHECK(expression::compile("${literal('2023-03-01T18:04:55-01:00'):toDate()}")(expression::Parameters()).asSignedLong() == expected_second.count());

  REQUIRE_THROWS_AS(expression::compile("${literal('2023-03-01T19:04:55Zbanana'):toDate()}")(expression::Parameters()), std::runtime_error);
  REQUIRE_THROWS_AS(expression::compile("${literal('2023-03-01T19:04:55'):toDate()}")(expression::Parameters()), std::runtime_error);
  REQUIRE_THROWS_AS(expression::compile("${literal('2023-03-01T19:04:55T'):toDate()}")(expression::Parameters()), std::runtime_error);
  REQUIRE_THROWS_AS(expression::compile("${literal('2023-03-01T19:04:55Z '):toDate()}")(expression::Parameters()), std::runtime_error);
  REQUIRE_THROWS_AS(expression::compile("${literal(' 2023-03-01T19:04:55Z'):toDate()}")(expression::Parameters()), std::runtime_error);
  REQUIRE_THROWS_AS(expression::compile("${literal('2023-03-01'):toDate()}")(expression::Parameters()), std::runtime_error);
}

TEST_CASE("Format Date", "[expressionFormatDate]") {
#ifdef WIN32
  expression::dateSetInstall(TZ_DATA_DIR);
#endif
  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("trillion_milliseconds", "1000000000000");
  CHECK(expression::compile("${trillion_milliseconds:format('%Y/%m/%d %H:%M:%SZ', 'UTC')}")(expression::Parameters{ flow_file_a.get() }).asString() == "2001/09/09 01:46:40.000Z");
  CHECK(expression::compile("${trillion_milliseconds:format('%Y/%m/%d %H:%M:%SZ', 'Europe/Paris')}")(expression::Parameters{ flow_file_a.get() }).asString() == "2001/09/09 03:46:40.000Z");
  CHECK(expression::compile("${trillion_milliseconds:format('%Y/%m/%d %H:%M:%SZ', 'America/Los_Angeles')}")(expression::Parameters{ flow_file_a.get() }).asString() == "2001/09/08 18:46:40.000Z");
}

TEST_CASE("IP", "[expressionIP]") {
  auto expr = expression::compile("${ip()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asString().empty());
}

TEST_CASE("Full Hostname", "[expressionFullHostname]") {
  auto expr = expression::compile("${hostname('true')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asString().empty());
}

TEST_CASE("Reverse DNS lookup with valid ip", "[ExpressionLanguage][reverseDnsLookup]") {
  auto expr = expression::compile("${reverseDnsLookup(${ip_addr})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  std::string expected_hostname;

  SECTION("dns.google IPv6") {
    if (minifi::test::utils::isIPv6Disabled())
      SKIP("IPv6 is disabled");
    flow_file_a->addAttribute("ip_addr", "2001:4860:4860::8888");
    expected_hostname = "dns.google";
  }

  SECTION("dns.google IPv4") {
    flow_file_a->addAttribute("ip_addr", "8.8.8.8");
    expected_hostname = "dns.google";
  }

  SECTION("Unresolvable address IPv6") {
    if (minifi::test::utils::isIPv6Disabled())
      SKIP("IPv6 is disabled");
    flow_file_a->addAttribute("ip_addr", "2001:db8::");
    expected_hostname = "2001:db8::";
  }

  SECTION("Unresolvable address IPv4") {
    flow_file_a->addAttribute("ip_addr", "192.0.2.0");
    expected_hostname = "192.0.2.0";
  }

  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asString() ==  expected_hostname);
}

TEST_CASE("Reverse DNS lookup with invalid ip", "[ExpressionLanguage][reverseDnsLookup]") {
  auto expr = expression::compile("${reverseDnsLookup(${ip_addr})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("ip_addr", "banana");

  REQUIRE_THROWS_AS(expr(expression::Parameters{flow_file_a.get()}), std::runtime_error);
}

TEST_CASE("Reverse DNS lookup with invalid timeout parameter", "[ExpressionLanguage][reverseDnsLookup]") {
  auto expr = expression::compile("${reverseDnsLookup(${ip_addr}, ${timeout})}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("ip_addr", "192.0.2.1");
  flow_file_a->addAttribute("timeout", "strawberry");

  REQUIRE_THROWS_AS(expr(expression::Parameters{ flow_file_a.get() }), std::invalid_argument);
}

TEST_CASE("Reverse DNS lookup with valid timeout parameter", "[ExpressionLanguage][reverseDnsLookup]") {
  LogTestController::getInstance().setWarn<expression::Expression>();
  LogTestController::getInstance().clear();

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  std::string expected_hostname;
  flow_file_a->addAttribute("ip_addr", "8.8.8.8");

  SECTION("Shouldn't timeout") {
    auto reverse_lookup_expr_500ms = expression::compile("${reverseDnsLookup(${ip_addr}, 500)}");
    CHECK(reverse_lookup_expr_500ms(expression::Parameters{flow_file_a.get()}).asString() == "dns.google");
    CHECK_FALSE(LogTestController::getInstance().contains("reverseDnsLookup timed out", 0ms));
  }
}

TEST_CASE("UUID", "[expressionUuid]") {
  auto expr = expression::compile("${UUID()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  REQUIRE(36 == expr(expression::Parameters{ flow_file_a.get() }).asString().length());
}

TEST_CASE("Trim", "[expressionTrim]") {
  auto expr = expression::compile("${message:trim()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", " 1 2 3 ");
  REQUIRE("1 2 3" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Append", "[expressionAppend]") {
  auto expr = expression::compile("${message:append('.gz')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt.gz" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Prepend", "[expressionPrepend]") {
  auto expr = expression::compile("${message:prepend('a brand new ')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Length", "[expressionLength]") {
  auto expr = expression::compile("${message:length()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "a brand new filename.txt");
  REQUIRE(24 == expr(expression::Parameters{ flow_file_a.get() }).asUnsignedLong());
}

TEST_CASE("Encode B64", "[expressionEncodeB64]") {
  auto expr = expression::compile("${message:base64Encode()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "admin:admin");
  REQUIRE("YWRtaW46YWRtaW4=" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Decode B64", "[expressionDecodeB64]") {
  auto expr = expression::compile("${message:base64Decode()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "YWRtaW46YWRtaW4=");
  REQUIRE("admin:admin" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Encode Decode B64", "[expressionEncodeDecodeB64]") {
  auto expr = expression::compile("${message:base64Encode():base64Decode()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("All Contains", "[expressionAllContains]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "hello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Contains 2", "[expressionAllContains2]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Contains", "[expressionAnyContains]") {
  auto expr = expression::compile("${anyAttribute('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Contains 2", "[expressionAnyContains2]") {
  auto expr = expression::compile("${anyAttribute('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Matching Contains", "[expressionAllMatchingContains]") {
  auto expr = expression::compile("${allMatchingAttributes('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "hello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Matching Contains 2", "[expressionAllMatchingContains2]") {
  auto expr = expression::compile("${allMatchingAttributes('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "hello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Matching Contains 3", "[expressionAllMatchingContains3]") {
  auto expr = expression::compile("${allMatchingAttributes('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("abc_2", "hello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Matching Contains 4", "[expressionAllMatchingContains4]") {
  auto expr = expression::compile("${allMatchingAttributes('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Matching Contains", "[expressionAnyMatchingContains]") {
  auto expr = expression::compile("${anyMatchingAttribute('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Matching Contains 2", "[expressionAnyMatchingContains2]") {
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Matching Contains 3", "[expressionAnyMatchingContains3]") {
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("abc_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Matching Contains 4", "[expressionAnyMatchingContains4]") {
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("xyz_1", "mello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Delineated Contains", "[expressionAllDelineatedContains]") {
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("word_list", "hello_1,hello_2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Delineated Contains 2", "[expressionAllDelineatedContains2]") {
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("word_list", "hello_1,mello_2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("All Delineated Contains 3", "[expressionAllDelineatedContains3]") {
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \" \"):contains('1,h')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("word_list", "hello_1,hello_2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Delineated Contains", "[expressionAnyDelineatedContains]") {
  auto expr = expression::compile("${anyDelineatedValue(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("word_list", "hello_1,mello_2");
  REQUIRE(expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Any Delineated Contains 2", "[expressionAnyDelineatedContains2]") {
  auto expr = expression::compile("${anyDelineatedValue(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("word_list", "mello_1,mello_2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a.get() }).asBoolean());
}

TEST_CASE("Count", "[expressionCount]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello'):count()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(1 == expr(expression::Parameters{ flow_file_a.get() }).asUnsignedLong());
}

TEST_CASE("Count 2", "[expressionCount2]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('mello'):count()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  flow_file_a->addAttribute("c", "hello 3");
  REQUIRE(2 == expr(expression::Parameters{ flow_file_a.get() }).asUnsignedLong());
}

TEST_CASE("Count 3", "[expressionCount3]") {
  auto expr = expression::compile("abc${allAttributes('a', 'b'):contains('mello'):count()}xyz");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  flow_file_a->addAttribute("c", "hello 3");
  REQUIRE("abc2xyz" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Join", "[expressionJoin]") {
  auto expr = expression::compile("abc_${allAttributes('a', 'b'):prepend('def_'):append('_ghi'):join(\"|\")}_xyz");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "hello");
  flow_file_a->addAttribute("b", "mello");
  REQUIRE("abc_def_hello_ghi|def_mello_ghi_xyz" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("Join 2", "[expressionJoin2]") {
  auto expr = expression::compile("abc_${allAttributes('a', 'b'):join(\"|\"):prepend('def_'):append('_ghi')}_xyz");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();
  flow_file_a->addAttribute("a", "hello");
  flow_file_a->addAttribute("b", "mello");
  REQUIRE("abc_def_hello|mello_ghi_xyz" == expr(expression::Parameters{ flow_file_a.get() }).asString());
}

TEST_CASE("resolve_user_id_test", "[resolve_user_id tests]") {
  auto expr = expression::compile("${attribute_sid:resolve_user_id()}");

  auto flow_file_a = std::make_shared<core::FlowFileImpl>();

  SECTION("TEST 0") {
  flow_file_a->addAttribute("attribute_sid", "0");
  REQUIRE("0" == expr(expression::Parameters{flow_file_a.get()}).asString());
}

  SECTION("TEST abcd") {
  flow_file_a->addAttribute("attribute_sid", "abcd");
  REQUIRE("abcd" == expr(expression::Parameters{flow_file_a.get()}).asString());
}

  SECTION("TEST empty") {
  flow_file_a->addAttribute("attribute_sid", "");
  REQUIRE(expr(expression::Parameters{flow_file_a.get()}).asString().empty());
}
}
