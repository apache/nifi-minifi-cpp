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
#include <filesystem>
#include <vector>
#include <string_view>
#include <string>
#include <functional>
#include <optional>
#include "utils/expected.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

struct LlamaChatMessage {
  std::string role;
  std::string content;
};

struct LlamaSamplerParams {
  std::optional<float> temperature;
  std::optional<int32_t> top_k;
  std::optional<float> top_p;
  std::optional<float> min_p;
  uint64_t min_keep{};
};

struct LlamaContextParams {
  uint32_t n_ctx{};
  uint32_t n_batch{};
  uint32_t n_ubatch{};
  uint32_t n_seq_max{};
  int32_t n_threads{};
  int32_t n_threads_batch{};
};

struct GenerationResult {
  std::chrono::milliseconds time_to_first_token{};
  uint64_t num_tokens_in{};
  uint64_t num_tokens_out{};
  double tokens_per_second{};
};

class LlamaContext {
 public:
  virtual std::optional<std::string> applyTemplate(const std::vector<LlamaChatMessage>& messages) = 0;
  virtual nonstd::expected<GenerationResult, std::string> generate(const std::string& input, std::function<void(std::string_view/*token*/)> token_handler) = 0;
  virtual ~LlamaContext() = default;
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
