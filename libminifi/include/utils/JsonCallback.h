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

#include <string>
#include <utility>
#include <memory>

#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "io/StreamPipe.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class JsonOutputCallback : public OutputStreamCallback {
 public:
  explicit JsonOutputCallback(rapidjson::Document&& root, utils::optional<uint8_t> decimal_places)
      : root_(std::move(root)), decimal_places_(decimal_places) {}

  int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (decimal_places_.has_value())
      writer.SetMaxDecimalPlaces(decimal_places_.value());
    root_.Accept(writer);
    return stream->write(reinterpret_cast<const uint8_t*>(buffer.GetString()), gsl::narrow<int>(buffer.GetSize()));
  }

 protected:
  rapidjson::Document root_;
  utils::optional<uint8_t> decimal_places_;
};

class PrettyJsonOutputCallback : public OutputStreamCallback {
 public:
  explicit PrettyJsonOutputCallback(rapidjson::Document&& root, utils::optional<uint8_t> decimal_places)
      : root_(std::move(root)), decimal_places_(decimal_places) {}

  int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    if (decimal_places_.has_value())
      writer.SetMaxDecimalPlaces(decimal_places_.value());
    root_.Accept(writer);
    return stream->write(reinterpret_cast<const uint8_t*>(buffer.GetString()), gsl::narrow<int>(buffer.GetSize()));
  }

 protected:
  rapidjson::Document root_;
  utils::optional<uint8_t> decimal_places_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
