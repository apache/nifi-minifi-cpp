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

#include "utils/LineByLineInputOutputStreamCallback.h"

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

LineByLineInputOutputStreamCallback::LineByLineInputOutputStreamCallback(CallbackType callback)
  : callback_(std::move(callback)) {
}

int64_t LineByLineInputOutputStreamCallback::process(const std::shared_ptr<io::BaseStream>& input, const std::shared_ptr<io::BaseStream>& output) {
  gsl_Expects(input);
  gsl_Expects(output);

  if (int64_t status = readInput(*input); status <= 0) {
    return status;
  }

  std::size_t total_bytes_written_ = 0;
  bool is_first_line = true;
  readLine();
  do {
    readLine();
    std::string output_line = callback_(*current_line_, is_first_line, isLastLine());
    output->write(reinterpret_cast<const uint8_t *>(output_line.data()), output_line.size());
    total_bytes_written_ += output_line.size();
    is_first_line = false;
  } while (!isLastLine());

  return gsl::narrow<int64_t>(total_bytes_written_);
}

int64_t LineByLineInputOutputStreamCallback::readInput(io::InputStream& stream) {
  const auto status = stream.read(input_, stream.size());
  if (io::isError(status)) { return -1; }
  current_pos_ = input_.begin();
  return gsl::narrow<int64_t>(input_.size());
}

void LineByLineInputOutputStreamCallback::readLine() {
  if (current_pos_ == input_.end()) {
    current_line_ = next_line_;
    next_line_ = std::nullopt;
    return;
  }

  auto end_of_line = std::find(current_pos_, input_.end(), '\n');
  if (end_of_line != input_.end()) { ++end_of_line; }

  current_line_ = next_line_;
  next_line_ = std::string(current_pos_, end_of_line);
  current_pos_ = end_of_line;
}

}  // namespace org::apache::nifi::minifi::utils
