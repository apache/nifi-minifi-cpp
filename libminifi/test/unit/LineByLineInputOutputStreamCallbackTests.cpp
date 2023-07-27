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
#include "LineByLineInputOutputStreamCallback.h"

#include "../TestBase.h"
#include "../Catch.h"
#include "core/logging/LoggerConfiguration.h"
#include "io/BufferStream.h"
#include "fmt/format.h"
#include "utils/span.h"

using minifi::utils::LineByLineInputOutputStreamCallback;

TEST_CASE("LineByLineInputOutputStreamCallback can process a stream line by line", "[process]") {
  const auto input_data = "One two, buckle my shoe\n"
                          "Three four, knock at the door\n"
                          "Five six, picking up sticks\n";
  const auto input_stream = std::make_shared<minifi::io::BufferStream>(input_data);
  const auto output_stream = std::make_shared<minifi::io::BufferStream>();

  LineByLineInputOutputStreamCallback::CallbackType line_processor;
  std::string expected_output;

  SECTION("no changes") {
    line_processor = [](const std::string& input_line, bool, bool) {
      return input_line;
    };
    expected_output = input_data;
  }
  SECTION("prepend asterisk") {
    line_processor = [](const std::string& input_line, bool, bool) {
      return "* " + input_line;
    };
    expected_output = "* One two, buckle my shoe\n"
                      "* Three four, knock at the door\n"
                      "* Five six, picking up sticks\n";
  }
  SECTION("replace vowels with underscores") {
    line_processor = [](const std::string& input_line, bool, bool) {
      return std::regex_replace(input_line, std::regex{"[aeiou]", std::regex::icase}, "_");
    };
    expected_output = "_n_ tw_, b_ckl_ my sh__\n"
                      "Thr__ f__r, kn_ck _t th_ d__r\n"
                      "F_v_ s_x, p_ck_ng _p st_cks\n";
  }
  SECTION("enclose input in square brackets") {
    line_processor = [](const std::string& input_line, bool is_first_line, bool is_last_line) {
      if (!is_first_line && !is_last_line) { return input_line; }
      auto output_line = input_line;
      if (is_first_line) { output_line = "[ " + output_line; }
      if (is_last_line) { output_line = output_line + " ]"; }
      return output_line;
    };
    expected_output = "[ One two, buckle my shoe\n"
                      "Three four, knock at the door\n"
                      "Five six, picking up sticks\n ]";
  }

  LineByLineInputOutputStreamCallback line_by_line_input_output_stream_callback{line_processor};
  line_by_line_input_output_stream_callback(input_stream, output_stream);
  const auto output_data = utils::span_to<std::string>(utils::as_span<const char>(output_stream->getBuffer()));
  CHECK(output_data == expected_output);
}

TEST_CASE("LineByLineInputOutputStreamCallback can handle Windows line endings", "[process][Windows]") {
  const auto input_data = "One two, buckle my shoe\r\n"
                          "Three four, knock at the door\r\n"
                          "Five six, picking up sticks\r\n";
  const auto input_stream = std::make_shared<minifi::io::BufferStream>(input_data);
  const auto output_stream = std::make_shared<minifi::io::BufferStream>();

  const auto line_processor = [](const std::string& input_line, bool, bool) {
    static int line_number = 0;
    return fmt::format("{0}: {1}", ++line_number, input_line);
  };
  const auto expected_output = "1: One two, buckle my shoe\r\n"
                               "2: Three four, knock at the door\r\n"
                               "3: Five six, picking up sticks\r\n";

  LineByLineInputOutputStreamCallback line_by_line_input_output_stream_callback{line_processor};
  line_by_line_input_output_stream_callback(input_stream, output_stream);
  const auto output_data = utils::span_to<std::string>(utils::as_span<const char>(output_stream->getBuffer()));
  CHECK(output_data == expected_output);
}

TEST_CASE("LineByLineInputOutputStreamCallback can handle an empty input", "[process][empty]") {
  const auto input_stream = std::make_shared<minifi::io::BufferStream>("");
  const auto output_stream = std::make_shared<minifi::io::BufferStream>();
  const auto line_processor = [](const std::string& input_line, bool, bool) { return input_line; };
  LineByLineInputOutputStreamCallback line_by_line_input_output_stream_callback{line_processor};
  line_by_line_input_output_stream_callback(input_stream, output_stream);
  CHECK(output_stream->size() == 0);
}
