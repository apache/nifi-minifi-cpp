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

#include "LlamaContext.h"
#include "llama.h"
#include "LlamaBackendInitializer.h"
#include "mtmd/mtmd.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

class DefaultLlamaContext : public LlamaContext {
 public:
  DefaultLlamaContext(const std::filesystem::path& model_path, const std::optional<std::filesystem::path>& multimodal_model_path,
      const LlamaSamplerParams& llama_sampler_params, const LlamaContextParams& llama_ctx_params, const std::shared_ptr<core::logging::Logger>& logger);
  DefaultLlamaContext(const DefaultLlamaContext&) = delete;
  DefaultLlamaContext(DefaultLlamaContext&&) = delete;
  DefaultLlamaContext& operator=(const DefaultLlamaContext&) = delete;
  DefaultLlamaContext& operator=(DefaultLlamaContext&&) = delete;
  ~DefaultLlamaContext() override;

  std::optional<std::string> applyTemplate(const std::vector<LlamaChatMessage>& messages) override;
  nonstd::expected<GenerationResult, std::string> generate(const std::string& prompt, const std::vector<std::vector<std::byte>>& files, std::function<void(std::string_view/*token*/)> token_handler) override;

 private:
  const LlamaBackendInitializer& llama_context_initializer_ = LlamaBackendInitializer::get();
  llama_model* llama_model_{};
  llama_context* llama_ctx_{};
  mtmd_context* multimodal_ctx_{};
  llama_sampler* llama_sampler_{};
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
