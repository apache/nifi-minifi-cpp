// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::utils {

class JsonInputCallback {
 public:
  explicit JsonInputCallback(rapidjson::Document& document) : document_(document) {}
  io::IoResult operator()(const std::shared_ptr<io::InputStream>& stream) {
    std::string content;
    content.resize(stream->size());
    const size_t read_ret = stream->read(as_writable_bytes(std::span(content)));
    if (io::isError(read_ret)) {
      return io::IoResult::error();
    }
    rapidjson::ParseResult parse_result = document_.Parse<rapidjson::kParseStopWhenDoneFlag>(content.data());
    if (parse_result.IsError()) {
      return io::IoResult::error();
    }

    return io::IoResult::from(read_ret);
  }
 private:
  rapidjson::Document& document_;
};

class JsonOutputCallback {
 public:
  explicit JsonOutputCallback(rapidjson::Document&& root, std::optional<uint8_t> decimal_places)
      : root_(std::move(root)), decimal_places_(decimal_places) {}

  io::IoResult operator()(const std::shared_ptr<io::OutputStream>& stream) const {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (decimal_places_.has_value())
      writer.SetMaxDecimalPlaces(decimal_places_.value());
    root_.Accept(writer);
    const auto write_return = stream->write(reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize());
    return io::IoResult::from(write_return);
  }

 protected:
  rapidjson::Document root_;
  std::optional<uint8_t> decimal_places_;
};

class PrettyJsonOutputCallback {
 public:
  explicit PrettyJsonOutputCallback(rapidjson::Document&& root, std::optional<uint8_t> decimal_places)
      : root_(std::move(root)), decimal_places_(decimal_places) {}

  io::IoResult operator()(const std::shared_ptr<io::OutputStream>& stream) const {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    if (decimal_places_.has_value())
      writer.SetMaxDecimalPlaces(decimal_places_.value());
    root_.Accept(writer);
    const auto write_return = stream->write(reinterpret_cast<const uint8_t*>(buffer.GetString()), buffer.GetSize());
    return io::IoResult::from(write_return);
  }

 protected:
  rapidjson::Document root_;
  std::optional<uint8_t> decimal_places_;
};

}  // namespace org::apache::nifi::minifi::utils
