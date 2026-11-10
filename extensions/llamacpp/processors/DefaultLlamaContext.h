// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "LlamaContext.h"
#include "llama.h"
#include "LlamaBackendInitializer.h"
#include "chat.h"
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
  std::expected<GenerationResult, std::string> generate(const std::string& prompt, const std::vector<std::vector<std::byte>>& files,
      std::function<void(std::string_view/*token*/)> token_handler) override;

 private:
  void initializeMultimodalContext(const std::filesystem::path& multimodal_model_path);
  std::expected<llama_pos, std::string> tokenizeAndDecodeMultimodalInput(const std::string& prompt, const std::vector<std::vector<std::byte>>& files);

  const LlamaBackendInitializer& llama_context_initializer_ = LlamaBackendInitializer::get();
  llama_model* llama_model_{};
  common_chat_templates_ptr chat_template_;
  llama_context* llama_ctx_{};
  mtmd_context* multimodal_ctx_{};
  llama_sampler* llama_sampler_{};
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
