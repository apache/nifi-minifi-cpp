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
#ifndef DISABLE_CURL
#ifdef WIN32
#pragma comment(lib, "libcurl.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "crypt32.lib")
#endif
#include <curl/curl.h>
#endif
#include "impl/expression/Expression.h"
#include <ExtractText.h>
#include <GetFile.h>
#include <PutFile.h>
#include <UpdateAttribute.h>
#include "core/FlowFile.h"
#include <LogAttribute.h>
#include "utils/gsl.h"
#include "TestBase.h"

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
  auto flow_file = std::make_shared<core::FlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${attr_a}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr(expression::Parameters{ flow_file }).asString());
}

TEST_CASE("Attribute expression (Null)", "[expressionLanguageTestAttributeExpressionNull]") {
  auto expr = expression::compile("text_before${attr_a}text_after");
  std::shared_ptr<core::FlowFile> flow_file = nullptr;
  REQUIRE("text_beforetext_after" == expr(expression::Parameters{ flow_file }).asString());
}

TEST_CASE("Multi-attribute expression", "[expressionLanguageTestMultiAttributeExpression]") {
  auto flow_file = std::make_shared<core::FlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  flow_file->addAttribute("attr_b", "__attr_value_b__");
  auto expr = expression::compile("text_before${attr_a}text_between${attr_b}text_after");
  REQUIRE("text_before__attr_value_a__text_between__attr_value_b__text_after" == expr(expression::Parameters{ flow_file }).asString());
}

TEST_CASE("Multi-flowfile attribute expression",
    "[expressionLanguageTestMultiFlowfileAttributeExpression]") {
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a }).asString());

  auto flow_file_b = std::make_shared<core::FlowFile>();
  flow_file_b->addAttribute("attr_a", "__flow_b_attr_value_a__");
  REQUIRE("text_before__flow_b_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_b }).asString());
}

TEST_CASE("Attribute expression with whitespace", "[expressionLanguageTestAttributeExpressionWhitespace]") {
  auto flow_file = std::make_shared<core::FlowFile>();
  flow_file->addAttribute("attr_a", "__attr_value_a__");
  auto expr = expression::compile("text_before${\n\tattr_a \r}text_after");
  REQUIRE("text_before__attr_value_a__text_after" == expr(expression::Parameters{ flow_file }).asString());
}

TEST_CASE("Special characters expression", "[expressionLanguageTestSpecialCharactersExpression]") {
  auto expr = expression::compile("text_before|{}()[],:;\\/*#'\" \t\r\n${attr_a}}()text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before|{}()[],:;\\/*#'\" \t\r\n__flow_a_attr_value_a__}()text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("UTF-8 characters expression", "[expressionLanguageTestUTF8Expression]") {
  auto expr = expression::compile("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹${attr_a}text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("UTF-8 characters attribute", "[expressionLanguageTestUTF8Attribute]") {
  auto expr = expression::compile("text_before${attr_a}text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr_a", "__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__");
  REQUIRE("text_before__¥£€¢₡₢₣₤₥₦₧₨₩₪₫₭₮₯₹__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Single quoted attribute expression", "[expressionLanguageTestSingleQuotedAttributeExpression]") {
  auto expr = expression::compile("text_before${'|{}()[],:;\\\\/*# \t\r\n$'}text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Double quoted attribute expression", "[expressionLanguageTestDoubleQuotedAttributeExpression]") {
  auto expr = expression::compile("text_before${\"|{}()[],:;\\\\/*# \t\r\n$\"}text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("|{}()[],:;\\/*# \t\r\n$", "__flow_a_attr_value_a__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Hostname function", "[expressionLanguageTestHostnameFunction]") {
  auto expr = expression::compile("text_before${\n\t hostname ()\n\t }text_after");

  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);
  std::string expected("text_before");
  expected.append(hostname);
  expected.append("text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  REQUIRE(expected == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("ToUpper function", "[expressionLanguageTestToUpperFunction]") {
  auto expr = expression::compile(R"(text_before${
                                       attr_a : toUpper()
                                     }text_after)");
  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("ToUpper function w/o whitespace", "[expressionLanguageTestToUpperFunctionWithoutWhitespace]") {
  auto expr = expression::compile(R"(text_before${attr_a:toUpper()}text_after)");
  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr_a", "__flow_a_attr_value_a__");
  REQUIRE("text_before__FLOW_A_ATTR_VALUE_A__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("ToLower function", "[expressionLanguageTestToLowerFunction]") {
  auto expr = expression::compile(R"(text_before${
                                       attr_a : toLower()
                                     }text_after)");
  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr_a", "__FLOW_A_ATTR_VALUE_A__");
  REQUIRE("text_before__flow_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("GetFile PutFile dynamic attribute", "[expressionLanguageTestGetFilePutFileDynamicAttribute]") {
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

  char format[] = "/tmp/gt.XXXXXX";
  std::string in_dir = testController.createTempDirectory(format);
  REQUIRE(!in_dir.empty());

  std::string in_file(in_dir);
  in_file.append("/file");
  char formatX[] = "/tmp/gt.XXXXXX";
  std::string out_dir = testController.createTempDirectory(formatX);
  REQUIRE(!out_dir.empty());

  std::string out_file(out_dir);
  out_file.append("/extracted_attr/file");

  // Build MiNiFi processing graph
  auto get_file = plan->addProcessor("GetFile", "GetFile");
  plan->setProperty(get_file, processors::GetFile::Directory.getName(), in_dir);
  plan->setProperty(get_file, processors::GetFile::KeepSourceFile.getName(), "false");
  auto update = plan->addProcessor("UpdateAttribute", "UpdateAttribute", core::Relationship("success", "description"), true);
  update->setDynamicProperty("prop_attr", "${'nifi.my.own.property'}_added");
  plan->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
  auto extract_text = plan->addProcessor("ExtractText", "ExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(extract_text, processors::ExtractText::Attribute.getName(), "extracted_attr_name");
  plan->addProcessor("LogAttribute", "LogAttribute", core::Relationship("success", "description"), true);
  auto put_file = plan->addProcessor("PutFile", "PutFile", core::Relationship("success", "description"), true);
  plan->setProperty(put_file, processors::PutFile::Directory.getName(), out_dir + "/${extracted_attr_name}");
  plan->setProperty(put_file, processors::PutFile::ConflictResolution.getName(), processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);
  plan->setProperty(put_file, processors::PutFile::CreateDirs.getName(), "true");

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

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Substring 1 arg", "[expressionLanguageSubstring1]") {
  auto expr = expression::compile("text_before${attr:substring(6)}text_after");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("text_before_a_attr_value_a__text_after" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Substring Before", "[expressionLanguageSubstringBefore]") {
  auto expr = expression::compile("${attr:substringBefore('attr_value_a__')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Substring Before Last", "[expressionLanguageSubstringBeforeLast]") {
  auto expr = expression::compile("${attr:substringBeforeLast('_a')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__flow_a_attr_value" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Substring After", "[expressionLanguageSubstringAfter]") {
  auto expr = expression::compile("${attr:substringAfter('__flow_a')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("_attr_value_a__" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Substring After Last", "[expressionLanguageSubstringAfterLast]") {
  auto expr = expression::compile("${attr:substringAfterLast('_a')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "__flow_a_attr_value_a__");
  REQUIRE("__" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Get Delimited", "[expressionLanguageGetDelimited]") {
  auto expr = expression::compile("${attr:getDelimitedField(2)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE(" 32" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Get Delimited 2", "[expressionLanguageGetDelimited2]") {
  auto expr = expression::compile("${attr:getDelimitedField(1)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE("\"Jacobson, John\"" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Get Delimited 3", "[expressionLanguageGetDelimited3]") {
  auto expr = expression::compile("${attr:getDelimitedField(1, ',', '\\\"', '\\\\', 'true')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "\"Jacobson, John\", 32, Mr.");
  REQUIRE("Jacobson, John" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Starts With", "[expressionLanguageStartsWith]") {
  auto expr = expression::compile("${attr:startsWith('a brand')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "A BRAND TEST");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Starts With 2", "[expressionLanguageStartsWith2]") {
  auto expr = expression::compile("${attr:startsWith('a brand')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand TEST");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Ends With", "[expressionLanguageEndsWith]") {
  auto expr = expression::compile("${attr:endsWith('txt')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.TXT");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Ends With 2", "[expressionLanguageEndsWith2]") {
  auto expr = expression::compile("${attr:endsWith('TXT')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.TXT");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Contains", "[expressionLanguageContains]") {
  auto expr = expression::compile("${attr:contains('new')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Contains 2", "[expressionLanguageContains2]") {
  auto expr = expression::compile("${attr:contains('NEW')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("In", "[expressionLanguageIn]") {
  auto expr = expression::compile("${attr:in('PAUL', 'JOHN', 'MIKE')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "JOHN");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("In 2", "[expressionLanguageIn2]") {
  auto expr = expression::compile("${attr:in('RED', 'GREEN', 'BLUE')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "JOHN");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Substring Before No Args", "[expressionLanguageSubstringBeforeNoArgs]") {
  REQUIRE_THROWS_WITH(expression::compile("${attr:substringBefore()}"), "Expression language function substringBefore called with 1 argument(s), but 2 are required");
}

TEST_CASE("Substring After No Args", "[expressionLanguageSubstringAfterNoArgs]") {
  REQUIRE_THROWS_WITH(expression::compile("${attr:substringAfter()}"), "Expression language function substringAfter called with 1 argument(s), but 2 are required");
}

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("Replace", "[expressionLanguageReplace]") {
  auto expr = expression::compile("${attr:replace('.', '_')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename_txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace 2", "[expressionLanguageReplace2]") {
  auto expr = expression::compile("${attr:replace(' ', '.')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a.brand.new.filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace First", "[expressionLanguageReplaceFirst]") {
  auto expr = expression::compile("${attr:replaceFirst('a', 'the')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("the brand new filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace First Regex", "[expressionLanguageReplaceFirstRegex]") {
  auto expr = expression::compile("${attr:replaceFirst('[br]', 'g')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a grand new filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace All", "[expressionLanguageReplaceAll]") {
  auto expr = expression::compile("${attr:replaceAll('\\\\..*', '')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace All 2", "[expressionLanguageReplaceAll2]") {
  auto expr = expression::compile("${attr:replaceAll('a brand (new)', '$1')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("new filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace All 3", "[expressionLanguageReplaceAll3]") {
  auto expr = expression::compile("${attr:replaceAll('XYZ', 'ZZZ')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace Null", "[expressionLanguageReplaceNull]") {
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace Null 2", "[expressionLanguageReplaceNull2]") {
  auto expr = expression::compile("${attr:replaceNull('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr2", "a brand new filename.txt");
  REQUIRE("abc" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace Empty", "[expressionLanguageReplaceEmpty]") {
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace Empty 2", "[expressionLanguageReplaceEmpty2]") {
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "  \t  \r  \n  ");
  REQUIRE("abc" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Replace Empty 3", "[expressionLanguageReplaceEmpty2]") {
  auto expr = expression::compile("${attr:replaceEmpty('abc')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr2", "test");
  REQUIRE("abc" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Matches", "[expressionLanguageMatches]") {
  auto expr = expression::compile("${attr:matches('^(Ct|Bt|At):.*t$')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "At:est");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Matches 2", "[expressionLanguageMatches2]") {
  auto expr = expression::compile("${attr:matches('^(Ct|Bt|At):.*t$')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "At:something");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Matches 3", "[expressionLanguageMatches3]") {
  auto expr = expression::compile("${attr:matches('(Ct|Bt|At):.*t')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", " At:est");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Find", "[expressionLanguageFind]") {
  auto expr = expression::compile("${attr:find('a [Bb]rand [Nn]ew')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Find 2", "[expressionLanguageFind2]") {
  auto expr = expression::compile("${attr:find('Brand.*')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Find 3", "[expressionLanguageFind3]") {
  auto expr = expression::compile("${attr:find('brand')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("IndexOf", "[expressionLanguageIndexOf]") {
  auto expr = expression::compile("${attr:indexOf('a.*txt')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("-1" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("IndexOf2", "[expressionLanguageIndexOf2]") {
  auto expr = expression::compile("${attr:indexOf('.')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("20" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("IndexOf3", "[expressionLanguageIndexOf3]") {
  auto expr = expression::compile("${attr:indexOf('a')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("0" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("IndexOf4", "[expressionLanguageIndexOf4]") {
  auto expr = expression::compile("${attr:indexOf(' ')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("1" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LastIndexOf", "[expressionLanguageLastIndexOf]") {
  auto expr = expression::compile("${attr:lastIndexOf('a.*txt')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("-1" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LastIndexOf2", "[expressionLanguageLastIndexOf2]") {
  auto expr = expression::compile("${attr:lastIndexOf('.')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("20" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LastIndexOf3", "[expressionLanguageLastIndexOf3]") {
  auto expr = expression::compile("${attr:lastIndexOf('a')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("17" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LastIndexOf4", "[expressionLanguageLastIndexOf4]") {
  auto expr = expression::compile("${attr:lastIndexOf(' ')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "a brand new filename.txt");
  REQUIRE("11" == expr(expression::Parameters{ flow_file_a }).asString());
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("Plus Integer", "[expressionLanguagePlusInteger]") {
  auto expr = expression::compile("${attr:plus(13)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("24" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Plus Decimal", "[expressionLanguagePlusDecimal]") {
  auto expr = expression::compile("${attr:plus(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-2.24567" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Plus Exponent", "[expressionLanguagePlusExponent]") {
  auto expr = expression::compile("${attr:plus(10e+6)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("10000011" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Plus Exponent 2", "[expressionLanguagePlusExponent2]") {
  auto expr = expression::compile("${attr:plus(10e+6)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11.345678901234");
  REQUIRE("10000011.345678901234351" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Minus Integer", "[expressionLanguageMinusInteger]") {
  auto expr = expression::compile("${attr:minus(13)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("-2" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Minus Decimal", "[expressionLanguageMinusDecimal]") {
  auto expr = expression::compile("${attr:minus(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("24.44567" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Multiply Integer", "[expressionLanguageMultiplyInteger]") {
  auto expr = expression::compile("${attr:multiply(13)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("143" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Multiply Decimal", "[expressionLanguageMultiplyDecimal]") {
  auto expr = expression::compile("${attr:multiply(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-148.136937" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Divide Integer", "[expressionLanguageDivideInteger]") {
  auto expr = expression::compile("${attr:divide(13)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11");
  REQUIRE("0.846153846153846" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Divide Decimal", "[expressionLanguageDivideDecimal]") {
  auto expr = expression::compile("${attr:divide(-13.34567)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "11.1");
  REQUIRE("-0.831730441409086" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("To Radix", "[expressionLanguageToRadix]") {
  auto expr = expression::compile("${attr:toRadix(2,16)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "10");
  REQUIRE("0000000000001010" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("To Radix 2", "[expressionLanguageToRadix2]") {
  auto expr = expression::compile("${attr:toRadix(16)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "13");
  REQUIRE("d" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("To Radix 3", "[expressionLanguageToRadix3]") {
  auto expr = expression::compile("${attr:toRadix(23,8)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "-2347");
  REQUIRE("-000004a1" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("From Radix", "[expressionLanguageFromRadix]") {
  auto expr = expression::compile("${attr:fromRadix(2)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "0000000000001010");
  REQUIRE("10" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("From Radix 2", "[expressionLanguageFromRadix2]") {
  auto expr = expression::compile("${attr:fromRadix(16)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "d");
  REQUIRE("13" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("From Radix 3", "[expressionLanguageFromRadix3]") {
  auto expr = expression::compile("${attr:fromRadix(23)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "-000004a1");
  REQUIRE("-2347" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Random", "[expressionLanguageRandom]") {
  auto expr = expression::compile("${random()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  auto result = expr(expression::Parameters{ flow_file_a }).asSignedLong();
  REQUIRE(result > 0);
}

TEST_CASE("Chained call", "[expressionChainedCall]") {
  auto expr = expression::compile("${attr:multiply(3):plus(1)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("22" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Chained call 2", "[expressionChainedCall2]") {
  auto expr = expression::compile("${literal(10):multiply(2):plus(1):multiply(2)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  REQUIRE(42 == expr(expression::Parameters{ flow_file_a }).asSignedLong());
}

TEST_CASE("Chained call 3", "[expressionChainedCall3]") {
  auto expr = expression::compile("${literal(10):multiply(2):plus(${attr:multiply(2)}):multiply(${attr})}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("238" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LiteralBool", "[expressionLiteralBool]") {
  auto expr = expression::compile("${literal(true)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE(true == expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("LiteralBool 2", "[expressionLiteralBool2]") {
  auto expr = expression::compile("${literal(false)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE(false == expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Is Null", "[expressionIsNull]") {
  auto expr = expression::compile("${filename:isNull()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Is Null 2", "[expressionIsNull2]") {
  auto expr = expression::compile("${filename:isNull()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "7");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Not Null", "[expressionNotNull]") {
  auto expr = expression::compile("${filename:notNull()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Not Null 2", "[expressionNotNull2]") {
  auto expr = expression::compile("${filename:notNull()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "7");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Is Empty", "[expressionIsEmpty]") {
  auto expr = expression::compile("${filename:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Is Empty 2", "[expressionIsEmpty2]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "7");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Is Empty 3", "[expressionIsEmpty3]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", " \t\r\n ");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Is Empty 4", "[expressionIsEmpty4]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Is Empty 5", "[expressionIsEmpty5]") {
  auto expr = expression::compile("${attr:isEmpty()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", " \t\r\n a \t\r\n ");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Equals", "[expressionEquals]") {
  auto expr = expression::compile("${attr:equals('hello.txt')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "hello.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Equals 2", "[expressionEquals2]") {
  auto expr = expression::compile("${attr:equals('hello.txt')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "helllo.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Equals 3", "[expressionEquals3]") {
  auto expr = expression::compile("${attr:plus(5):equals(6)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Equals Ignore Case", "[expressionEqualsIgnoreCase]") {
  auto expr = expression::compile("${attr:equalsIgnoreCase('hElLo.txt')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "hello.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Equals Ignore Case 2", "[expressionEqualsIgnoreCase2]") {
  auto expr = expression::compile("${attr:plus(5):equalsIgnoreCase(6)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("GT", "[expressionGT]") {
  auto expr = expression::compile("${attr:plus(5):gt(5)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("GT2", "[expressionGT2]") {
  auto expr = expression::compile("${attr:plus(5.1):gt(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("GT3", "[expressionGT3]") {
  auto expr = expression::compile("${attr:plus(5.1):gt(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("GE", "[expressionGE]") {
  auto expr = expression::compile("${attr:plus(5):ge(6)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("GE2", "[expressionGE2]") {
  auto expr = expression::compile("${attr:plus(5.1):ge(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("GE3", "[expressionGE3]") {
  auto expr = expression::compile("${attr:plus(5.1):ge(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LT", "[expressionLT]") {
  auto expr = expression::compile("${attr:plus(5):lt(5)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LT2", "[expressionLT2]") {
  auto expr = expression::compile("${attr:plus(5.1):lt(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LT3", "[expressionLT3]") {
  auto expr = expression::compile("${attr:plus(5.1):lt(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LE", "[expressionLE]") {
  auto expr = expression::compile("${attr:plus(5):le(6)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LE2", "[expressionLE2]") {
  auto expr = expression::compile("${attr:plus(5.1):le(6.05)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("LE3", "[expressionLE3]") {
  auto expr = expression::compile("${attr:plus(5.1):le(6.15)}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("attr", "1");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("And", "[expressionAnd]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('an')})}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("And 2", "[expressionAnd2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('ab')})}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Or", "[expressionOr]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):or(${filename:substring(0, 2):equals('an')})}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Or 2", "[expressionOr2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):or(${filename:substring(0, 2):equals('ab')})}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Not", "[expressionNot]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('an')}):not()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("false" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Not 2", "[expressionNot2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename} ):and(${filename:substring(0, 2):equals('ab')}):not()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("true" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("If Else", "[expressionIfElse]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename}):ifElse('yes', 'no')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "an example file.txt");
  REQUIRE("yes" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("If Else 2", "[expressionIfElse2]") {
  auto expr = expression::compile("${filename:toLower():equals( ${filename}):ifElse('yes', 'no')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("filename", "An example file.txt");
  REQUIRE("no" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode JSON", "[expressionEncodeJSON]") {
  auto expr = expression::compile("${message:escapeJson()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "This is a \"test!\"");
  REQUIRE("This is a \\\"test!\\\"" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode JSON", "[expressionDecodeJSON]") {
  auto expr = expression::compile("${message:unescapeJson()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "This is a \\\"test!\\\"");
  REQUIRE("This is a \"test!\"" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode Decode JSON", "[expressionEncodeDecodeJSON]") {
  auto expr = expression::compile("${message:escapeJson():unescapeJson()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "This is a \"test!\"");
  REQUIRE("This is a \"test!\"" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode XML", "[expressionEncodeXML]") {
  auto expr = expression::compile("${message:escapeXml()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero &gt; One &lt; &quot;two!&quot; &amp; &apos;true&apos;" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode XML", "[expressionDecodeXML]") {
  auto expr = expression::compile("${message:unescapeXml()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "Zero &gt; One &lt; &quot;two!&quot; &amp; &apos;true&apos;");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode Decode XML", "[expressionEncodeDecodeXML]") {
  auto expr = expression::compile("${message:escapeXml():unescapeXml()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode HTML3", "[expressionEncodeHTML3]") {
  auto expr = expression::compile("${message:escapeHtml3()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "¥ & < «");
  REQUIRE("&yen; &amp; &lt; &laquo;" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode HTML3", "[expressionDecodeHTML3]") {
  auto expr = expression::compile("${message:unescapeHtml3()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &laquo;");
  REQUIRE("¥ & < «" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode Decode HTML3", "[expressionEncodeDecodeHTML3]") {
  auto expr = expression::compile("${message:escapeHtml3():unescapeHtml3()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &laquo;");
  REQUIRE("&yen; &amp; &lt; &laquo;" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode HTML4", "[expressionEncodeHTML4]") {
  auto expr = expression::compile("${message:escapeHtml4()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "¥ & Φ < «");
  REQUIRE("&yen; &amp; &Phi; &lt; &laquo;" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode HTML4", "[expressionDecodeHTML4]") {
  auto expr = expression::compile("${message:unescapeHtml4()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "&yen; &iota; &amp; &lt; &laquo;");
  REQUIRE("¥ ι & < «" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode Decode HTML4", "[expressionEncodeDecodeHTML4]") {
  auto expr = expression::compile("${message:escapeHtml4():unescapeHtml4()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "&yen; &amp; &lt; &Pi; &laquo;");
  REQUIRE("&yen; &amp; &lt; &Pi; &laquo;" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode CSV", "[expressionEncodeCSV]") {
  auto expr = expression::compile("${message:escapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("\"Zero > One < \"\"two!\"\" & 'true'\"" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode CSV", "[expressionDecodeCSV]") {
  auto expr = expression::compile("${message:unescapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", R"("Zero > One < ""two!"" & 'true'")");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode CSV 2", "[expressionDecodeCSV2]") {
  auto expr = expression::compile("${message:unescapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", R"("quoted")");
  REQUIRE("\"quoted\"" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode Decode CSV", "[expressionEncodeDecodeCSV]") {
  auto expr = expression::compile("${message:escapeCsv():unescapeCsv()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a }).asString());
}

#ifndef WIN32
#ifndef DISABLE_CURL
TEST_CASE("Encode URL", "[expressionEncodeURL]") {
  auto expr = expression::compile("${message:urlEncode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE("some%20value%20with%20spaces" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode URL", "[expressionDecodeURL]") {
  auto expr = expression::compile("${message:urlDecode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "some%20value%20with%20spaces");
  REQUIRE("some value with spaces" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode Decode URL", "[expressionEncodeDecodeURL]") {
  auto expr = expression::compile("${message:urlEncode():urlDecode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE("some value with spaces" == expr(expression::Parameters{ flow_file_a }).asString());
}
#else
TEST_CASE("Encode URL", "[expressionEncodeURLExcept]") {
  auto expr = expression::compile("${message:urlEncode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE_THROWS(expr(expression::Parameters{flow_file_a}).asString());
}

TEST_CASE("Decode URL", "[expressionDecodeURLExcept]") {
  auto expr = expression::compile("${message:urlDecode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "some%20value%20with%20spaces");
  REQUIRE_THROWS(expr(expression::Parameters{flow_file_a}).asString());
}

TEST_CASE("Encode Decode URL", "[expressionEncodeDecodeURLExcept]") {
  auto expr = expression::compile("${message:urlEncode():urlDecode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "some value with spaces");
  REQUIRE_THROWS(expr(expression::Parameters{flow_file_a}).asString());
}
#endif
#endif

#ifdef EXPRESSION_LANGUAGE_USE_DATE

TEST_CASE("Parse Date", "[expressionParseDate]") {
  auto expr = expression::compile("${message:toDate('%Y/%m/%d', 'America/Los_Angeles')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "2014/04/30");
  REQUIRE("1398841200000" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Reformat Date", "[expressionReformatDate]") {
  auto expr = expression::compile("${message:toDate('%Y/%m/%d', 'GMT'):format('%m-%d-%Y', 'America/New_York')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "2014/03/14");
  REQUIRE("03-13-2014" == expr(expression::Parameters{ flow_file_a }).asString());
}

#endif  // EXPRESSION_LANGUAGE_USE_DATE

TEST_CASE("Now Date", "[expressionNowDate]") {
  auto expr = expression::compile("${now():format('%Y')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "2014/03/14");
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);

  REQUIRE(gsl::narrow<uint64_t>(lt.tm_year + 1900) == expr(expression::Parameters{ flow_file_a }).asUnsignedLong());
}

TEST_CASE("Format Date", "[expressionFormatDate]") {
  auto expr_gmt = expression::compile("${message:format('%m-%d-%Y', 'GMT')}");
  auto expr_utc = expression::compile("${message:format('%m-%d-%Y', 'UTC')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "1394755200000");
  REQUIRE("03-14-2014" == expr_gmt(expression::Parameters{ flow_file_a }).asString());
  REQUIRE("03-14-2014" == expr_utc(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("IP", "[expressionIP]") {
  auto expr = expression::compile("${ip()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  REQUIRE("" != expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Full Hostname", "[expressionFullHostname]") {
  auto expr = expression::compile("${hostname('true')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  REQUIRE("" != expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("UUID", "[expressionUuid]") {
  auto expr = expression::compile("${UUID()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  REQUIRE(36 == expr(expression::Parameters{ flow_file_a }).asString().length());
}

TEST_CASE("Trim", "[expressionTrim]") {
  auto expr = expression::compile("${message:trim()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", " 1 2 3 ");
  REQUIRE("1 2 3" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Append", "[expressionAppend]") {
  auto expr = expression::compile("${message:append('.gz')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "a brand new filename.txt");
  REQUIRE("a brand new filename.txt.gz" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Prepend", "[expressionPrepend]") {
  auto expr = expression::compile("${message:prepend('a brand new ')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "filename.txt");
  REQUIRE("a brand new filename.txt" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Length", "[expressionLength]") {
  auto expr = expression::compile("${message:length()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "a brand new filename.txt");
  REQUIRE(24 == expr(expression::Parameters{ flow_file_a }).asUnsignedLong());
}

TEST_CASE("Encode B64", "[expressionEncodeB64]") {
  auto expr = expression::compile("${message:base64Encode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "admin:admin");
  REQUIRE("YWRtaW46YWRtaW4=" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Decode B64", "[expressionDecodeB64]") {
  auto expr = expression::compile("${message:base64Decode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "YWRtaW46YWRtaW4=");
  REQUIRE("admin:admin" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Encode Decode B64", "[expressionEncodeDecodeB64]") {
  auto expr = expression::compile("${message:base64Encode():base64Decode()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("message", "Zero > One < \"two!\" & 'true'");
  REQUIRE("Zero > One < \"two!\" & 'true'" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("All Contains", "[expressionAllContains]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "hello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("All Contains 2", "[expressionAllContains2]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Contains", "[expressionAnyContains]") {
  auto expr = expression::compile("${anyAttribute('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Contains 2", "[expressionAnyContains2]") {
  auto expr = expression::compile("${anyAttribute('a', 'b'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

#ifdef EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("All Matching Contains", "[expressionAllMatchingContains]") {
  auto expr = expression::compile("${allMatchingAttributes('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "hello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("All Matching Contains 2", "[expressionAllMatchingContains2]") {
  auto expr = expression::compile("${allMatchingAttributes('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "hello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("All Matching Contains 3", "[expressionAllMatchingContains3]") {
  auto expr = expression::compile("${allMatchingAttributes('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("abc_2", "hello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("All Matching Contains 4", "[expressionAllMatchingContains4]") {
  auto expr = expression::compile("${allMatchingAttributes('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Matching Contains", "[expressionAnyMatchingContains]") {
  auto expr = expression::compile("${anyMatchingAttribute('xyz_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Matching Contains 2", "[expressionAnyMatchingContains2]") {
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("xyz_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Matching Contains 3", "[expressionAnyMatchingContains3]") {
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("abc_1", "hello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Matching Contains 4", "[expressionAnyMatchingContains4]") {
  auto expr = expression::compile("${anyMatchingAttribute('abc_.*'):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("xyz_1", "mello 1");
  flow_file_a->addAttribute("xyz_2", "mello 2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

#endif  // EXPRESSION_LANGUAGE_USE_REGEX

TEST_CASE("All Delineated Contains", "[expressionAllDelineatedContains]") {
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,hello_2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("All Delineated Contains 2", "[expressionAllDelineatedContains2]") {
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,mello_2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("All Delineated Contains 3", "[expressionAllDelineatedContains3]") {
  auto expr = expression::compile("${allDelineatedValues(${word_list}, \" \"):contains('1,h')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,hello_2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Delineated Contains", "[expressionAnyDelineatedContains]") {
  auto expr = expression::compile("${anyDelineatedValue(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("word_list", "hello_1,mello_2");
  REQUIRE(expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Any Delineated Contains 2", "[expressionAnyDelineatedContains2]") {
  auto expr = expression::compile("${anyDelineatedValue(${word_list}, \",\"):contains('hello')}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("word_list", "mello_1,mello_2");
  REQUIRE(!expr(expression::Parameters{ flow_file_a }).asBoolean());
}

TEST_CASE("Count", "[expressionCount]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('hello'):count()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "hello 1");
  flow_file_a->addAttribute("b", "mello 2");
  REQUIRE(1 == expr(expression::Parameters{ flow_file_a }).asUnsignedLong());
}

TEST_CASE("Count 2", "[expressionCount2]") {
  auto expr = expression::compile("${allAttributes('a', 'b'):contains('mello'):count()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  flow_file_a->addAttribute("c", "hello 3");
  REQUIRE(2 == expr(expression::Parameters{ flow_file_a }).asUnsignedLong());
}

TEST_CASE("Count 3", "[expressionCount3]") {
  auto expr = expression::compile("abc${allAttributes('a', 'b'):contains('mello'):count()}xyz");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "mello 1");
  flow_file_a->addAttribute("b", "mello 2");
  flow_file_a->addAttribute("c", "hello 3");
  REQUIRE("abc2xyz" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Join", "[expressionJoin]") {
  auto expr = expression::compile("abc_${allAttributes('a', 'b'):prepend('def_'):append('_ghi'):join(\"|\")}_xyz");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "hello");
  flow_file_a->addAttribute("b", "mello");
  REQUIRE("abc_def_hello_ghi|def_mello_ghi_xyz" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("Join 2", "[expressionJoin2]") {
  auto expr = expression::compile("abc_${allAttributes('a', 'b'):join(\"|\"):prepend('def_'):append('_ghi')}_xyz");

  auto flow_file_a = std::make_shared<core::FlowFile>();
  flow_file_a->addAttribute("a", "hello");
  flow_file_a->addAttribute("b", "mello");
  REQUIRE("abc_def_hello|mello_ghi_xyz" == expr(expression::Parameters{ flow_file_a }).asString());
}

TEST_CASE("resolve_user_id_test", "[resolve_user_id tests]") {
  auto expr = expression::compile("${attribute_sid:resolve_user_id()}");

  auto flow_file_a = std::make_shared<core::FlowFile>();

  SECTION("TEST 0") {
  flow_file_a->addAttribute("attribute_sid", "0");
  REQUIRE("0" == expr(expression::Parameters{flow_file_a}).asString());
}

  SECTION("TEST abcd") {
  flow_file_a->addAttribute("attribute_sid", "abcd");
  REQUIRE("abcd" == expr(expression::Parameters{flow_file_a}).asString());
}

  SECTION("TEST empty") {
  flow_file_a->addAttribute("attribute_sid", "");
  REQUIRE("" == expr(expression::Parameters{flow_file_a}).asString());
}
}

