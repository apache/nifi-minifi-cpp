// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  virtual std::expected<GenerationResult, std::string> generate(const std::string& input, const std::vector<std::vector<std::byte>>& files,
      std::function<void(std::string_view/*token*/)> token_handler) = 0;
  virtual ~LlamaContext() = default;
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
