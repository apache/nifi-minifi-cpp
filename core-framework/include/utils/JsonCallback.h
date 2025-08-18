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
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "io/StreamPipe.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class JsonInputCallback {
 public:
  explicit JsonInputCallback(rapidjson::Document& document) : document_(document) {}
  int64_t operator()(const std::shared_ptr<io::InputStream>& stream) {
    std::string content;
    content.resize(stream->size());
    const auto read_ret = stream->read(as_writable_bytes(std::span(content)));
    if (io::isError(read_ret)) {
      return -1;
    }
    rapidjson::ParseResult parse_result = document_.Parse<rapidjson::kParseStopWhenDoneFlag>(content.data());
    if (parse_result.IsError())
      return -1;

    return read_ret;
  }
 private:
  rapidjson::Document& document_;
};

class JsonOutputCallback {
 public:
  explicit JsonOutputCallback(rapidjson::Document&& root, std::optional<uint8_t> decimal_places)
      : root_(std::move(root)), decimal_places_(decimal_places) {}

  int64_t operator()(const std::shared_ptr<io::OutputStream>& stream) const {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (decimal_places_.has_value())
      writer.SetMaxDecimalPlaces(decimal_places_.value());
    root_.Accept(writer);
    const auto write_return = stream->write(reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize());
    return !io::isError(write_return) ? gsl::narrow<int64_t>(write_return) : -1;
  }

 protected:
  rapidjson::Document root_;
  std::optional<uint8_t> decimal_places_;
};

class PrettyJsonOutputCallback {
 public:
  explicit PrettyJsonOutputCallback(rapidjson::Document&& root, std::optional<uint8_t> decimal_places)
      : root_(std::move(root)), decimal_places_(decimal_places) {}

  int64_t operator()(const std::shared_ptr<io::OutputStream>& stream) const {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    if (decimal_places_.has_value())
      writer.SetMaxDecimalPlaces(decimal_places_.value());
    root_.Accept(writer);
    const auto write_return = stream->write(reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize());
    return !io::isError(write_return) ? gsl::narrow<int64_t>(write_return) : -1;
  }

 protected:
  rapidjson::Document root_;
  std::optional<uint8_t> decimal_places_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
