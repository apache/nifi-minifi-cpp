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

#include "DefaultLlamaContext.h"
#include "api/Exception.h"
#include "fmt/format.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

namespace {
std::vector<llama_token> tokenizeInput(const llama_vocab* vocab, const std::string& input) {
  int32_t number_of_tokens = gsl::narrow<int32_t>(input.length()) + 2;
  std::vector<llama_token> tokenized_input(number_of_tokens);
  number_of_tokens = llama_tokenize(vocab, input.data(), gsl::narrow<int32_t>(input.length()), tokenized_input.data(), gsl::narrow<int32_t>(tokenized_input.size()), true, true);
  if (number_of_tokens < 0) {
    tokenized_input.resize(-number_of_tokens);
    [[maybe_unused]] int32_t check = llama_tokenize(vocab, input.data(), gsl::narrow<int32_t>(input.length()), tokenized_input.data(), gsl::narrow<int32_t>(tokenized_input.size()), true, true);
    gsl_Assert(check == -number_of_tokens);
  } else {
    tokenized_input.resize(number_of_tokens);
  }
  return tokenized_input;
}
}  // namespace


DefaultLlamaContext::DefaultLlamaContext(const std::filesystem::path& model_path, const LlamaSamplerParams& llama_sampler_params, const LlamaContextParams& llama_ctx_params) {
  llama_model_ = llama_model_load_from_file(model_path.string().c_str(), llama_model_default_params());  // NOLINT(cppcoreguidelines-prefer-member-initializer)
  if (!llama_model_) {
    throw api::Exception(api::ExceptionType::PROCESS_SCHEDULE_EXCEPTION, fmt::format("Failed to load model from '{}'", model_path.string()));
  }

  llama_context_params ctx_params = llama_context_default_params();
  ctx_params.n_ctx = llama_ctx_params.n_ctx;
  ctx_params.n_batch = llama_ctx_params.n_batch;
  ctx_params.n_ubatch = llama_ctx_params.n_ubatch;
  ctx_params.n_seq_max = llama_ctx_params.n_seq_max;
  ctx_params.n_threads = llama_ctx_params.n_threads;
  ctx_params.n_threads_batch = llama_ctx_params.n_threads_batch;
  ctx_params.flash_attn = false;
  llama_ctx_ = llama_init_from_model(llama_model_, ctx_params);

  auto sparams = llama_sampler_chain_default_params();
  llama_sampler_ = llama_sampler_chain_init(sparams);

  if (llama_sampler_params.min_p) {
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_min_p(*llama_sampler_params.min_p, llama_sampler_params.min_keep));
  }
  if (llama_sampler_params.top_k) {
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_k(*llama_sampler_params.top_k));
  }
  if (llama_sampler_params.top_p) {
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_p(*llama_sampler_params.top_p, llama_sampler_params.min_keep));
  }
  if (llama_sampler_params.temperature) {
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_temp(*llama_sampler_params.temperature));
  }
  llama_sampler_chain_add(llama_sampler_, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}

DefaultLlamaContext::~DefaultLlamaContext() {
  llama_sampler_free(llama_sampler_);
  llama_sampler_ = nullptr;
  llama_free(llama_ctx_);
  llama_ctx_ = nullptr;
  llama_model_free(llama_model_);
  llama_model_ = nullptr;
}

std::optional<std::string> DefaultLlamaContext::applyTemplate(const std::vector<LlamaChatMessage>& messages) {
  std::vector<llama_chat_message> llama_messages;
  llama_messages.reserve(messages.size());
  std::transform(messages.begin(), messages.end(), std::back_inserter(llama_messages),
                 [](const LlamaChatMessage& msg) { return llama_chat_message{.role = msg.role.c_str(), .content = msg.content.c_str()}; });
  std::string text;
  text.resize(4096);
  const char * chat_template = llama_model_chat_template(llama_model_, nullptr);
  int32_t res_size = llama_chat_apply_template(chat_template, llama_messages.data(), llama_messages.size(), true, text.data(), gsl::narrow<int32_t>(text.size()));
  if (res_size < 0) {
    return std::nullopt;
  }
  if (res_size > gsl::narrow<int32_t>(text.size())) {
    text.resize(res_size);
    res_size = llama_chat_apply_template(chat_template, llama_messages.data(), llama_messages.size(), true, text.data(), gsl::narrow<int32_t>(text.size()));
    if (res_size < 0) {
      return std::nullopt;
    }
  }
  text.resize(res_size);

  return text;
}

nonstd::expected<GenerationResult, std::string> DefaultLlamaContext::generate(const std::string& input, std::function<void(std::string_view/*token*/)> token_handler) {
  GenerationResult result{};
  auto start_time = std::chrono::steady_clock::now();
  const llama_vocab * vocab = llama_model_get_vocab(llama_model_);
  std::vector<llama_token> tokenized_input = tokenizeInput(vocab, input);
  result.num_tokens_in = gsl::narrow<uint64_t>(tokenized_input.size());

  llama_batch batch = llama_batch_get_one(tokenized_input.data(), gsl::narrow<int32_t>(tokenized_input.size()));
  llama_token new_token_id = 0;
  bool first_token_generated = false;
  while (true) {
    int32_t res = llama_decode(llama_ctx_, batch);
    if (res == 1) {
      return nonstd::make_unexpected("Could not find a KV slot for the batch (try reducing the size of the batch or increase the context)");
    } else if (res < 0) {
      return nonstd::make_unexpected("Error occurred while executing llama decode");
    }

    new_token_id = llama_sampler_sample(llama_sampler_, llama_ctx_, -1);
    if (!first_token_generated) {
      result.time_to_first_token = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
      first_token_generated = true;
    }

    if (llama_vocab_is_eog(vocab, new_token_id)) {
      break;
    }

    ++result.num_tokens_out;
    llama_sampler_accept(llama_sampler_, new_token_id);

    std::array<char, 128> buf{};
    int32_t len = llama_token_to_piece(vocab, new_token_id, buf.data(), gsl::narrow<int32_t>(buf.size()), 0, true);
    if (len < 0) {
      return nonstd::make_unexpected("Failed to convert token to text");
    }
    gsl_Assert(len < 128);

    std::string_view token_str{buf.data(), gsl::narrow<std::string_view::size_type>(len)};
    batch = llama_batch_get_one(&new_token_id, 1);
    token_handler(token_str);
  }

  result.tokens_per_second =
    gsl::narrow<double>(result.num_tokens_out) / (gsl::narrow<double>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count()) / 1000.0);
  return result;
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
