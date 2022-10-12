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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/logging/Logger.h"
#include "io/InputStream.h"
#include "io/OutputStream.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi::utils {

class LineByLineInputOutputStreamCallback {
 public:
  using CallbackType = std::function<std::string(const std::string& input_line, bool is_first_line, bool is_last_line)>;
  explicit LineByLineInputOutputStreamCallback(CallbackType callback);
  int64_t operator()(const std::shared_ptr<io::InputStream>& input, const std::shared_ptr<io::OutputStream>& output);

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
