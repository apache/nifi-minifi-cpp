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

#include "catch.hpp"

#include <vector>
#include <string>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "core/string_utils.h"
#include "core/file_utils.h"


void test_lists_equal(token_list * tknlist, const std::vector<std::string>& sv) {
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

std::string join_strings(const std::vector<std::string>& strings, const std::string& token) {
    if (strings.empty()) {
        return "";
    }

    if (strings.size() == 1) {
        return strings.at(0);
    }
    const auto& initialValue = strings.at(0);
    return std::accumulate(strings.begin()+1, strings.end(), initialValue,
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
    std::string delimitedString = "";
    char delim = '-';
    struct token_list tokens = tokenize_string(delimitedString.c_str(), delim);
    REQUIRE(tokens.size == 0);
}

TEST_CASE("Test string tokenizer normal delimited string", "[stringTokenizerDelimitedString]") {
    std::vector<std::string> slist = {"this", "is a", "delimited", "string"};
    std::string delimitedString = join_strings(slist, "-");
    char delim = '-';
    struct token_list tokens = tokenize_string(delimitedString.c_str(), delim);
    REQUIRE(tokens.size == 4);
    REQUIRE(validate_list(&tokens) == 1);
    test_lists_equal(&tokens, slist);
}

TEST_CASE("Test string tokenizer delimiter started string", "[stringTokenizerDelimiterStartedString]") {
    std::vector<std::string> slist = {"", "this", "is", "a test"};
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

    std::vector<std::string> slist = {"this", "is a", "delimited", "string", ""};
    std::string delimitedString = join_strings(slist, "-");
    int len = strlen(delimitedString.c_str());
    char delim = '-';
    struct token_list tokens = tokenize_string_tailfile(delimitedString.c_str(), delim);
    REQUIRE(tokens.has_non_delimited_token == 0);
    REQUIRE(tokens.size == 4);
    REQUIRE(validate_list(&tokens) == 1);
    slist.pop_back();
    test_lists_equal(&tokens, slist);
}

TEST_CASE("Test string tokenizer for a non-delimited string less than 4096 bytes", "[testNonDelimitedStringTokenizer]") {
    std::string nonDelimitedString = "this is a non delimited string";
    char delim = '-';
    struct token_list tokens = tokenize_string_tailfile(nonDelimitedString.c_str(), delim);
    REQUIRE(tokens.has_non_delimited_token == 0);
    REQUIRE(tokens.size == 0);
    test_lists_equal(&tokens, {});
}

TEST_CASE("Test string tokenizer for empty string", "[testEmptyStringTokenizer]") {
    const std::string emptyString = "";
    char delim = '-';
    struct token_list tokens = tokenize_string_tailfile(emptyString.c_str(), delim);
    REQUIRE(tokens.has_non_delimited_token == 0);
    REQUIRE(tokens.size == 0);
    test_lists_equal(&tokens, {});
}

TEST_CASE("Test string tokenizer for string containing only delimited characters", "[testDelimiterCharOnlyStringTokenizer]") {
    const std::string str = "----";
    char delim = '-';
    struct token_list tokens = tokenize_string_tailfile(str.c_str(), delim);
    REQUIRE(tokens.has_non_delimited_token == 0);
    REQUIRE(tokens.size == 0);
    test_lists_equal(&tokens, {});
}

TEST_CASE("Test string tokenizer for string starting with delimited characters", "[testDelimitedStartingStringTokenizer]") {
    const std::string str = "----mystring";
    char delim = '-';
    struct token_list tokens = tokenize_string_tailfile(str.c_str(), delim);
    REQUIRE(tokens.has_non_delimited_token == 0);
    REQUIRE(tokens.size == 0);
    test_lists_equal(&tokens, {});
}

TEST_CASE("Test string tokenizer for string starting and ending with delimited characters", "[testDelimitedStartingEndingStringTokenizer]") {
    const std::string str = "---token1---token2---token3";
    char delim = '-';
    struct token_list tokens = tokenize_string_tailfile(str.c_str(), delim);
    REQUIRE(tokens.has_non_delimited_token == 0);
    REQUIRE(tokens.size == 2);
    test_lists_equal(&tokens, std::vector<std::string>{"token1", "token2"});
}

/****
 * ##################################################################
 *  CTAILFILE TAIL TESTS
 * ##################################################################
 */

class FileManager {
public:
    FileManager(const std::string& filePath) {
        assert(!filePath.empty() && "filePath provided cannot be empty!");
        filePath_ = filePath;
        outputStream_.open(filePath_, std::ios::binary);
    }

    ~FileManager() {
        std::ifstream ifs(filePath_);
        if (ifs.good()) {
            remove(filePath_.c_str());
        }
    }

    void Write(const std::string& str) {
        outputStream_ << str;
    }

    std::string WriteNChars(uint64_t n, char c) {
        std::string s(n, c);
        outputStream_ << s;
        return s;
    }

    std::string getFilePath() const {
        return filePath_;
    }

    void CloseStream() {
        outputStream_.flush();
        outputStream_.close();
    }

    uint64_t GetFileSize() {
        CloseStream();
        struct stat buff;
        if (stat(filePath_.c_str(), &buff) == 0) {
            return buff.st_size;
        }
        return 0;
    }

private:
    std::string filePath_;
    std::ofstream outputStream_;
};

TEST_CASE("Simple tail file test", "[testTailFile]") {

    FileManager fm("test.txt");
    fm.Write("hello world");
    fm.CloseStream();

    const char * file = fm.getFilePath().c_str();
    struct token_list tkn_list = tail_file(file, ';', 0);
    REQUIRE(tkn_list.size == 0);
    REQUIRE(tkn_list.head == NULL);
    REQUIRE(tkn_list.total_bytes == 0);
}

TEST_CASE("Empty file tail test", "[testEmptyFileTail]") {
    FileManager fm("test.txt");
    fm.CloseStream();

    const char * file = fm.getFilePath().c_str();
    struct token_list tkn_list = tail_file(file, ';', 0);
    REQUIRE(tkn_list.size == 0);
    REQUIRE(tkn_list.head == NULL);
    REQUIRE(tkn_list.total_bytes == 0);
}

TEST_CASE("File containing only delimiters tail test", "[testDelimiterOnlyFileTail]") {
    FileManager fm("test.txt");
    fm.Write("----");
    fm.CloseStream();

    const char * file = fm.getFilePath().c_str();
    struct token_list tkn_list = tail_file(file, '-', 0);
    REQUIRE(tkn_list.size == 0);
    REQUIRE(tkn_list.head == NULL);
    REQUIRE(tkn_list.total_bytes == 4);
}

TEST_CASE("File tail test string starting with delimiter", "[testDelimiterOnlyFileTail]") {
    FileManager fm("test.txt");
    fm.Write("----hello");
    fm.CloseStream();

    const char * file = fm.getFilePath().c_str();
    struct token_list tkn_list = tail_file(file, '-', 0);
    REQUIRE(tkn_list.size == 0);
    REQUIRE(tkn_list.head == NULL);
    REQUIRE(tkn_list.total_bytes == 4);
}

TEST_CASE("Test tail file with less than 4096 delimited chars", "[testTailFileDelimitedString]") {

    const std::string delimitedString = "token1--token2--token3";
    FileManager fm("test.txt");
    fm.Write(delimitedString);
    const std::string filePath = fm.getFilePath();
    fm.CloseStream();

    struct token_list tokens = tail_file(filePath.c_str(), '-', 0);
    test_lists_equal(&tokens, std::vector<std::string>{"token1", "token2"});
}

// Although there is no delimiter within the string that is at least 4096 bytes long,
// tail_file still creates a flow file for the first 4096 bytes.
TEST_CASE("Test tail file having 4096 bytes without delimiter", "[testTailFile4096Chars]") {

    FileManager fm("test.txt");
    const std::string s = std::move(fm.WriteNChars(4096, 'a'));
    const std::string filePath = fm.getFilePath();
    fm.CloseStream();

    struct token_list tokens = tail_file(filePath.c_str(), '-', 0);
    test_lists_equal(&tokens, std::vector<std::string>{std::move(s)});
}

// Although there is no delimiter within the string that is equal to 4096 bytes or longer
// tail_file creates a flow file for each subsequent 4096 byte chunk. It leaves the last chunk
// if it is smaller than 4096 bytes and not delimited
TEST_CASE("Test tail file having more than 4096 bytes without delimiter", "[testTailFileMoreThan4096Chars]") {

    FileManager fm("test.txt");
    const std::string s1 = std::move(fm.WriteNChars(4096, 'a'));
    const std::string s2 = std::move(fm.WriteNChars(4096, 'b'));
    const std::string s3 = "helloworld";
    fm.Write(s3);
    fm.CloseStream();

    const uint64_t totalStringsSize = s1.size() + s2.size() + s3.size();
    const std::string filePath = fm.getFilePath();
    const uint64_t bytesWrittenToStream = fm.GetFileSize();
    REQUIRE(bytesWrittenToStream == totalStringsSize);

    struct token_list tokens = tail_file(filePath.c_str(), '-', 0);
    test_lists_equal(&tokens, std::vector<std::string>{std::move(s1), std::move(s2)});
}

TEST_CASE("Test tail file having more than 4096 bytes with delimiter", "[testTailFileWithDelimitedString]") {

    FileManager fm("test.txt");
    const std::string s1 = std::move(fm.WriteNChars(4096, 'a'));
    const std::string d1 = std::move(fm.WriteNChars(2, '-'));
    const std::string s2 = std::move(fm.WriteNChars(4096, 'b'));
    fm.CloseStream();

    const uint64_t totalStringsSize = s1.size() + s2.size() + d1.size();
    const std::string filePath = fm.getFilePath();
    const uint64_t bytesWrittenToStream = fm.GetFileSize();
    REQUIRE(bytesWrittenToStream == totalStringsSize);

    struct token_list tokens = tail_file(filePath.c_str(), '-', 0);
    test_lists_equal(&tokens, std::vector<std::string>{std::move(s1), std::move(s2)});
}

TEST_CASE("Test tail file having more than 4096 bytes with delimiter and second chunk less than 4096", "[testTailFileWithDelimitedString]") {

    FileManager fm("test.txt");
    const std::string s1 = std::move(fm.WriteNChars(4096, 'a'));
    const std::string d1 = std::move(fm.WriteNChars(2, '-'));
    const std::string s2 = std::move(fm.WriteNChars(4000, 'b'));
    fm.CloseStream();

    const uint64_t totalStringsSize = s1.size() + s2.size() + d1.size();
    const std::string filePath = fm.getFilePath();
    const uint64_t bytesWrittenToStream = fm.GetFileSize();
    REQUIRE(bytesWrittenToStream == totalStringsSize);

    struct token_list tokens = tail_file(filePath.c_str(), '-', 0);
    test_lists_equal(&tokens, std::vector<std::string>{std::move(s1)});
}

TEST_CASE("Test tail file having more than 4096 bytes with delimiter and second chunk more than 4096", "[testTailFileWithDelimitedString]") {

    FileManager fm("test.txt");
    const std::string s1 = std::move(fm.WriteNChars(4096, 'a'));
    const std::string d1 = std::move(fm.WriteNChars(2, '-'));
    const std::string s2 = std::move(fm.WriteNChars(4098, 'b'));
    fm.CloseStream();

    const uint64_t totalStringsSize = s1.size() + s2.size() + d1.size();
    const std::string filePath = fm.getFilePath();
    const uint64_t bytesWrittenToStream = fm.GetFileSize();
    REQUIRE(bytesWrittenToStream == totalStringsSize);

    const std::string s3 = std::string(s2.data(), s2.data()+4096);
    struct token_list tokens = tail_file(filePath.c_str(), '-', 0);
    test_lists_equal(&tokens, std::vector<std::string>{std::move(s1), std::move(s3)});
}
