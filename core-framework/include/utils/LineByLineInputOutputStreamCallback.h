// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/io/OutputStream.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::utils {

class LineByLineInputOutputStreamCallback {
 public:
  using CallbackType = std::function<std::string(const std::string& input_line, bool is_first_line, bool is_last_line)>;
  explicit LineByLineInputOutputStreamCallback(CallbackType callback);
  io::ReadWriteResult operator()(const std::shared_ptr<io::InputStream>& input, const std::shared_ptr<io::OutputStream>& output);

 private:
  int64_t readInput(io::InputStream& stream);
  void readLine();
  [[nodiscard]] bool isLastLine() const { return !next_line_.has_value(); }

  CallbackType callback_;
  std::vector<std::byte> input_;
  std::vector<std::byte>::iterator current_pos_{};
  std::optional<std::string> current_line_;
  std::optional<std::string> next_line_;
};

}  // namespace org::apache::nifi::minifi::utils
