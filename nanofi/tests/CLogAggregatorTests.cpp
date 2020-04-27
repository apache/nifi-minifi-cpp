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

#ifndef _WIN32
#include "catch.hpp"

#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "core/string_utils.h"
#include "core/file_utils.h"

#include "CTestsBase.h"

void test_lists_equal(token_list * tknlist,
    const std::vector<std::string>& sv) {
  REQUIRE(tknlist != NULL);
  if (sv.empty()) {
    REQUIRE(tknlist->head == NULL);
    REQUIRE(tknlist->size == 0);
    return;
  }
  REQUIRE(tknlist->size == sv.size());
  for (const auto& s : sv) {
    if (tknlist->head) {
      REQUIRE(strcmp(s.c_str(), tknlist->head->data) == 0);
      tknlist->head = tknlist->head->next;
    }
  }
}

std::string join_strings(const std::vector<std::string>& strings,
    const std::string& token) {
  if (strings.empty()) {
    return "";
  }

  if (strings.size() == 1) {
    return strings.at(0);
  }
  const auto& initialValue = strings.at(0);
  return std::accumulate(strings.begin() + 1, strings.end(), initialValue,
      [token,initialValue](const std::string& s1, const std::string& s2) {
        return s1 + token + s2;
      });
}

/****
 * ##################################################################
 *  CTAILFILE STRING TOKENIZER TESTS
 * ##################################################################
 */

TEST_CASE("Test string tokenizer empty string", "[stringTokenizerEmptyString]") {
  std::string delimitedString;
  char delim = '-';
  struct token_list tokens = tokenize_string(delimitedString.c_str(), delim);
  REQUIRE(tokens.size == 0);
}

TEST_CASE("Test string tokenizer normal delimited string", "[stringTokenizerDelimitedString]") {
  std::vector<std::string> slist = { "this", "is a", "delimited", "string" };
  std::string delimitedString = join_strings(slist, "-");
  char delim = '-';
  struct token_list tokens = tokenize_string(delimitedString.c_str(), delim);
  REQUIRE(tokens.size == 4);
  REQUIRE(validate_list(&tokens) == 1);
  test_lists_equal(&tokens, slist);
}

TEST_CASE("Test string tokenizer delimiter started string", "[stringTokenizerDelimiterStartedString]") {
  std::vector<std::string> slist = { "", "this", "is", "a test" };
  std::string delimitedString = join_strings(slist, "--");
  char delim = '-';
  struct token_list tokens = tokenize_string(delimitedString.c_str(), delim);
  REQUIRE(tokens.size == 3);
  REQUIRE(validate_list(&tokens) == 1);
  slist.erase(std::remove(slist.begin(), slist.end(), ""), slist.end());
  test_lists_equal(&tokens, slist);
}

TEST_CASE("Test string tokenizer only delimiter character string", "[stringTokenizerDelimiterOnlyString]") {
  std::string delimitedString = "--------";
  char delim = '-';
  struct token_list tokens = tokenize_string(delimitedString.c_str(), delim);
  REQUIRE(tokens.size == 0);
}

/****
 * ##################################################################
 *  CTAILFILE TAIL FILE BEHAVED DELIMITED STRING TOKENIZER TESTS
 * ##################################################################
 */

TEST_CASE("Test string tokenizer for a delimited string less than 4096 bytes", "[testDelimitedStringTokenizer]") {

  std::vector<std::string> slist = { "this", "is a", "delimited", "string", "" };
  std::string delimitedString = join_strings(slist, "-");
  int len = strlen(delimitedString.c_str());
  char delim = '-';
  struct token_list tokens = tokenize_string_tailfile(delimitedString.c_str(),
      delim);
  REQUIRE(tokens.has_non_delimited_token == 0);
  REQUIRE(tokens.size == 4);
  REQUIRE(validate_list(&tokens) == 1);
  slist.pop_back();
  test_lists_equal(&tokens, slist);
}

TEST_CASE("Test string tokenizer for a non-delimited string less than 4096 bytes", "[testNonDelimitedStringTokenizer]") {
  std::string nonDelimitedString = "this is a non delimited string";
  char delim = '-';
  struct token_list tokens = tokenize_string_tailfile(
      nonDelimitedString.c_str(), delim);
  REQUIRE(tokens.has_non_delimited_token == 0);
  REQUIRE(tokens.size == 0);
  test_lists_equal(&tokens, { });
}

TEST_CASE("Test string tokenizer for empty string", "[testEmptyStringTokenizer]") {
  const std::string emptyString = "";
  char delim = '-';
  struct token_list tokens = tokenize_string_tailfile(emptyString.c_str(),
      delim);
  REQUIRE(tokens.has_non_delimited_token == 0);
  REQUIRE(tokens.size == 0);
  test_lists_equal(&tokens, { });
}

TEST_CASE("Test string tokenizer for string containing only delimited characters", "[testDelimiterCharOnlyStringTokenizer]") {
  const std::string str = "----";
  char delim = '-';
  struct token_list tokens = tokenize_string_tailfile(str.c_str(), delim);
  REQUIRE(tokens.has_non_delimited_token == 0);
  REQUIRE(tokens.size == 0);
  test_lists_equal(&tokens, { });
}

TEST_CASE("Test string tokenizer for string starting with delimited characters", "[testDelimitedStartingStringTokenizer]") {
  const std::string str = "----mystring";
  char delim = '-';
  struct token_list tokens = tokenize_string_tailfile(str.c_str(), delim);
  REQUIRE(tokens.has_non_delimited_token == 0);
  REQUIRE(tokens.size == 0);
  test_lists_equal(&tokens, { });
}

TEST_CASE("Test string tokenizer for string starting and ending with delimited characters", "[testDelimitedStartingEndingStringTokenizer]") {
  const std::string str = "---token1---token2---token3";
  char delim = '-';
  struct token_list tokens = tokenize_string_tailfile(str.c_str(), delim);
  REQUIRE(tokens.has_non_delimited_token == 0);
  REQUIRE(tokens.size == 2);
  test_lists_equal(&tokens, std::vector<std::string> { "token1", "token2" });
}

/****
 * ##################################################################
 *  CTAILFILE TAIL TESTS
 * ##################################################################
 */

TEST_CASE("Simple log aggregator test", "[testLogAggregator]") {

  const char * content = "hello world";
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);

  FileManager fm(file_path);
  fm.Write(content);
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  struct processor_params * pp = invoke_processor(mgr, file_path);

  REQUIRE(pp != NULL);
  REQUIRE(pp->curr_offset == 0);
  REQUIRE(flow_files_size(pp->ff_list) == 0);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Empty file log aggregator test", "[testEmptyFileLogAggregator]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  struct processor_params * pp = invoke_processor(mgr, file_path);

  REQUIRE(pp != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 0);
  REQUIRE(pp->ff_list == NULL);
  REQUIRE(pp->curr_offset == 0);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("File containing only delimiters test", "[testDelimiterOnlyLogAggregator]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  fm.Write(";;;;");
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  struct processor_params * pp = invoke_processor(mgr, file_path);

  REQUIRE(pp != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 0);
  REQUIRE(pp->ff_list == NULL);
  REQUIRE(pp->curr_offset == 4);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("File containing string starting with delimiter", "[testDelimiterStartingStrings]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  fm.Write(";;;;hello");
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  auto pp = invoke_processor(mgr, file_path);

  REQUIRE(flow_files_size(pp->ff_list) == 0);
  REQUIRE(pp->ff_list == NULL);
  REQUIRE(pp->curr_offset == 4);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tail file with less than 4096 delimited chars", "[testLogAggregateFileLessThan4KB]") {
  const std::string token1("token1");
  const std::string token2("token2");
  const std::string token3("token3");
  std::vector<std::string> tokens = { token1, token2, token3 };

  const std::string delimitedString = join_strings(tokens, ";;");
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  fm.Write(delimitedString);
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  auto pp = invoke_processor(mgr, file_path);

  REQUIRE(pp->curr_offset == (token1.size() + token2.size() + (2 * std::string("--").size())));
  REQUIRE(pp->ff_list != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 2);

  remove_temp_directory(file_path);
  free(file_path);
}

// Although there is no delimiter within the string that is at least 4096 bytes long,
// tail_file still creates a flow file for the first 4096 bytes.
TEST_CASE("Test tail file having 4096 bytes without delimiter", "[testLogAggregateFile4096Chars]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  const std::string s = fm.WriteNChars(4096, 'a');
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  auto pp = invoke_processor(mgr, file_path);
  REQUIRE(pp->curr_offset == 4096);
  REQUIRE(pp->ff_list != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 1);

  remove_temp_directory(file_path);
  free(file_path);
}

// Although there is no delimiter within the string that is equal to 4096 bytes or longer
// tail_file creates a flow file for each subsequent 4096 byte chunk. It leaves the last chunk
// if it is smaller than 4096 bytes and not delimited
TEST_CASE("Test tail file having more than 4096 bytes without delimiter", "[testLogAggregarteFileMoreThan4096Chars]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  const std::string s1 = fm.WriteNChars(4096, 'a');
  const std::string s2 = fm.WriteNChars(4096, 'b');
  const std::string s3 = "helloworld";
  fm.Write(s3);
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  const uint64_t totalStringsSize = s1.size() + s2.size() + s3.size();
  const uint64_t bytesWrittenToStream = fm.GetFileSize();
  REQUIRE(bytesWrittenToStream == totalStringsSize);

  auto pp = invoke_processor(mgr, file_path);
  REQUIRE(flow_files_size(pp->ff_list) == 2);
  flow_file_list * el = NULL;
  LL_FOREACH(pp->ff_list, el) {
    REQUIRE(el->ff_record->size == 4096);
  }
  REQUIRE(pp->curr_offset == (s1.size() + s2.size()));

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tail file having more than 4096 bytes with delimiter", "[testLogAggregateWithDelimitedString]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  const std::string s1 = fm.WriteNChars(4096, 'a');
  const std::string d1 = fm.WriteNChars(2, ';');
  const std::string s2 = fm.WriteNChars(4096, 'b');
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  const uint64_t totalStringsSize = s1.size() + s2.size() + d1.size();
  const uint64_t bytesWrittenToStream = fm.GetFileSize();
  REQUIRE(bytesWrittenToStream == totalStringsSize);

  auto pp = invoke_processor(mgr, file_path);
  REQUIRE(flow_files_size(pp->ff_list) == 2);
  REQUIRE(pp->curr_offset == totalStringsSize);
  flow_file_list * el = NULL;
  LL_FOREACH(pp->ff_list, el) {
    REQUIRE(el->ff_record->size == 4096);
  }

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tail file having more than 4096 bytes with delimiter and second chunk less than 4096", "[testLogAggregateDelimited]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  const std::string s1 = fm.WriteNChars(4096, 'a');
  const std::string d1 = fm.WriteNChars(2, ';');
  const std::string s2 = fm.WriteNChars(4000, 'b');
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  const uint64_t totalStringsSize = s1.size() + s2.size() + d1.size();
  const uint64_t bytesWrittenToStream = fm.GetFileSize();
  REQUIRE(bytesWrittenToStream == totalStringsSize);

  auto pp = invoke_processor(mgr, file_path);
  REQUIRE(pp->curr_offset == (s1.size() + d1.size()));
  REQUIRE(flow_files_size(pp->ff_list) == 1);

  flow_file_list * el = NULL;
  LL_FOREACH(pp->ff_list, el) {
    REQUIRE(el->ff_record->size == 4096);
  }

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tail file having more than 4096 bytes with delimiter and second chunk more than 4096", "[testLogAggregateDelimited]") {
  char * file_path = get_temp_file_path("test.txt");
  REQUIRE(file_path != NULL);
  FileManager fm(file_path);
  const std::string s1 = fm.WriteNChars(4096, 'a');
  const std::string d1 = fm.WriteNChars(2, ';');
  const std::string s2 = fm.WriteNChars(4098, 'b');
  fm.CloseStream();

  TailFileTestResourceManager mgr("LogAggregator", on_trigger_logaggregator);
  const uint64_t totalStringsSize = s1.size() + s2.size() + d1.size();
  const uint64_t bytesWrittenToStream = fm.GetFileSize();
  REQUIRE(bytesWrittenToStream == totalStringsSize);

  auto pp = invoke_processor(mgr, file_path);
  REQUIRE(pp->curr_offset == 8194);
  REQUIRE(flow_files_size(pp->ff_list) == 2);

  flow_file_list * el = NULL;
  LL_FOREACH(pp->ff_list, el) {
    REQUIRE(el->ff_record->size == 4096);
  }

  remove_temp_directory(file_path);
  free(file_path);
}
#endif
