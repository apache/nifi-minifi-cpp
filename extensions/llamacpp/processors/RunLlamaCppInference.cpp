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

#include "RunLlamaCppInference.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "minifi-cpp/Exception.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "LlamaContext.h"
#include "utils/ProcessorConfigUtils.h"
#include "DefaultLlamaContext.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

void RunLlamaCppInference::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void RunLlamaCppInference::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  model_path_.clear();
  model_path_ = utils::parseProperty(context, ModelPath);
  system_prompt_ = context.getProperty(SystemPrompt).value_or("");

  LlamaSamplerParams llama_sampler_params;
  llama_sampler_params.temperature = utils::parseOptionalFloatProperty(context, Temperature);
  if (auto top_k = utils::parseOptionalI64Property(context, TopK)) {
    llama_sampler_params.top_k = gsl::narrow<int32_t>(*top_k);
  }
  llama_sampler_params.top_p = utils::parseOptionalFloatProperty(context, TopP);
  llama_sampler_params.min_p = utils::parseOptionalFloatProperty(context, MinP);
  llama_sampler_params.min_keep = utils::parseU64Property(context, MinKeep);

  LlamaContextParams llama_ctx_params;
  llama_ctx_params.n_ctx = gsl::narrow<uint32_t>(utils::parseU64Property(context, TextContextSize));
  llama_ctx_params.n_batch = gsl::narrow<uint32_t>(utils::parseU64Property(context, LogicalMaximumBatchSize));
  llama_ctx_params.n_ubatch = gsl::narrow<uint32_t>(utils::parseU64Property(context, PhysicalMaximumBatchSize));
  llama_ctx_params.n_seq_max = gsl::narrow<uint32_t>(utils::parseU64Property(context, MaxNumberOfSequences));
  llama_ctx_params.n_threads = gsl::narrow<int32_t>(utils::parseI64Property(context, ThreadsForGeneration));
  llama_ctx_params.n_threads_batch = gsl::narrow<int32_t>(utils::parseI64Property(context, ThreadsForBatchProcessing));

  if (llama_context_provider_) {
    llama_ctx_ = llama_context_provider_(model_path_, llama_sampler_params, llama_ctx_params);
  } else {
    llama_ctx_ = std::make_unique<DefaultLlamaContext>(model_path_, llama_sampler_params, llama_ctx_params);
  }
}

void RunLlamaCppInference::increaseTokensIn(uint64_t token_count) {
  auto* const llamacpp_metrics = dynamic_cast<RunLlamaCppInferenceMetrics*>(metrics_.get());
  gsl_Assert(llamacpp_metrics);
  llamacpp_metrics->tokens_in += token_count;
}

void RunLlamaCppInference::increaseTokensOut(uint64_t token_count) {
  auto* const llamacpp_metrics = dynamic_cast<RunLlamaCppInferenceMetrics*>(metrics_.get());
  gsl_Assert(llamacpp_metrics);
  llamacpp_metrics->tokens_out += token_count;
}

void RunLlamaCppInference::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto prompt = context.getProperty(Prompt, flow_file.get()).value_or("");

  auto read_result = session.readBuffer(flow_file);
  std::string input_data_and_prompt;
  if (!read_result.buffer.empty()) {
    input_data_and_prompt.append("Input data (or flow file content):\n");
    input_data_and_prompt.append({reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()});
    input_data_and_prompt.append("\n\n");
  }
  input_data_and_prompt.append(prompt);

  if (input_data_and_prompt.empty()) {
    logger_->log_error("Input data and prompt are empty");
    session.transfer(flow_file, Failure);
    return;
  }

  auto input = [&] {
    std::vector<LlamaChatMessage> messages;
    if (!system_prompt_.empty()) {
      messages.push_back({.role = "system", .content = system_prompt_});
    }
    messages.push_back({.role = "user", .content = input_data_and_prompt});

    return llama_ctx_->applyTemplate(messages);
  }();

  if (!input) {
    logger_->log_error("Inference failed with while applying template");
    session.transfer(flow_file, Failure);
    return;
  }

  logger_->log_debug("AI model input: {}", *input);

  auto start_time = std::chrono::steady_clock::now();

  std::string text;
  auto generation_result = llama_ctx_->generate(*input, [&] (std::string_view token) {
    text += token;
  });

  auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();

  if (!generation_result) {
    logger_->log_error("Inference failed with generation error: '{}'", generation_result.error());
    session.transfer(flow_file, Failure);
    return;
  }

  increaseTokensIn(generation_result->num_tokens_in);
  increaseTokensOut(generation_result->num_tokens_out);

  logger_->log_debug("Number of tokens generated: {}", generation_result->num_tokens_out);
  logger_->log_debug("AI model inference time: {} ms", elapsed_time);
  logger_->log_debug("AI model output: {}", text);

  flow_file->setAttribute(LlamaCppTimeToFirstToken.name, std::to_string(generation_result->time_to_first_token.count()) + " ms");
  flow_file->setAttribute(LlamaCppTokensPerSecond.name, fmt::format("{:.2f}", generation_result->tokens_per_second));

  session.writeBuffer(flow_file, text);
  session.transfer(flow_file, Success);
}

void RunLlamaCppInference::notifyStop() {
  llama_ctx_.reset();
}

REGISTER_RESOURCE(RunLlamaCppInference, Processor);

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
