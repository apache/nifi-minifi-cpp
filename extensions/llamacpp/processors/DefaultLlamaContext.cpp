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

#include <ranges>

#include "minifi-cpp/Exception.h"
#include "fmt/format.h"
#include "mtmd/mtmd-helper.h"

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


DefaultLlamaContext::DefaultLlamaContext(const std::filesystem::path& model_path, const std::optional<std::filesystem::path>& multimodal_model_path,
    const LlamaSamplerParams& llama_sampler_params, const LlamaContextParams& llama_ctx_params, const std::shared_ptr<core::logging::Logger>& logger) {
  llama_model_ = llama_model_load_from_file(model_path.string().c_str(), llama_model_default_params());  // NOLINT(cppcoreguidelines-prefer-member-initializer)
  if (!llama_model_) {
    throw Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, fmt::format("Failed to load model from '{}'", model_path.string()));
  }

  chat_template_ = common_chat_templates_init(llama_model_, "");

  llama_context_params ctx_params = llama_context_default_params();
  ctx_params.n_ctx = llama_ctx_params.n_ctx;
  ctx_params.n_batch = llama_ctx_params.n_batch;
  ctx_params.n_ubatch = llama_ctx_params.n_ubatch;
  ctx_params.n_seq_max = llama_ctx_params.n_seq_max;
  ctx_params.n_threads = llama_ctx_params.n_threads;
  ctx_params.n_threads_batch = llama_ctx_params.n_threads_batch;
  ctx_params.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_DISABLED;
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

  if (!multimodal_model_path) {
    logger->log_info("No multimodal model path provided");
    return;
  }

  mtmd_context_params mparams = mtmd_context_params_default();
  mparams.use_gpu = false;
  mparams.flash_attn_type  = LLAMA_FLASH_ATTN_TYPE_DISABLED;

  multimodal_ctx_ = mtmd_init_from_file(multimodal_model_path->string().c_str(), llama_model_, mparams);
  if (!multimodal_ctx_) {
    throw Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, fmt::format("Failed to load multimodal model from '{}'", multimodal_model_path->string()));
  }

  logger->log_info("Successfully loaded multimodal model from '{}'", multimodal_model_path->string());
}

DefaultLlamaContext::~DefaultLlamaContext() {
  mtmd_free(multimodal_ctx_);
  multimodal_ctx_ = nullptr;
  llama_sampler_free(llama_sampler_);
  llama_sampler_ = nullptr;
  llama_free(llama_ctx_);
  llama_ctx_ = nullptr;
  llama_model_free(llama_model_);
  llama_model_ = nullptr;
}

std::optional<std::string> DefaultLlamaContext::applyTemplate(const std::vector<LlamaChatMessage>& messages) {
  if (!chat_template_) {
    return std::nullopt;
  }
  common_chat_templates_inputs inputs;
  for (auto& msg : messages) {
    common_chat_msg chat_msg;
    chat_msg.role = msg.role;
    chat_msg.content = msg.content;
    inputs.messages.push_back(std::move(chat_msg));
  }
  inputs.enable_thinking = false;  // TODO(adebreceni): MINIFICPP-2800 common_chat_templates_support_enable_thinking(chat_template_.get());

  return common_chat_templates_apply(chat_template_.get(), inputs).prompt;
}

namespace {

struct mtmd_bitmap_deleter {
  void operator()(mtmd_bitmap* val) { mtmd_bitmap_free(val); }
};
using unique_bitmap_ptr = std::unique_ptr<mtmd_bitmap, mtmd_bitmap_deleter>;

struct mtmd_input_chunks_deleter {
  void operator()(mtmd_input_chunks* val) { mtmd_input_chunks_free(val); }
};
using unique_mtmd_input_chunks_ptr = std::unique_ptr<mtmd_input_chunks, mtmd_input_chunks_deleter>;

class unique_llama_batch {
 public:
  explicit unique_llama_batch(std::optional<llama_batch> batch = std::nullopt): batch_(batch) {}

  unique_llama_batch(unique_llama_batch&&) = default;
  unique_llama_batch& operator=(unique_llama_batch&&) = default;
  unique_llama_batch(const unique_llama_batch&) = delete;
  unique_llama_batch& operator=(const unique_llama_batch&) = delete;

  [[nodiscard]] std::optional<llama_batch> get() const {
    return batch_;
  }

  std::optional<llama_batch>& operator->() {
    return batch_;
  }

  void reset(std::optional<llama_batch> batch = std::nullopt) {
    if (batch_) {
      llama_batch_free(batch_.value());
    }
    batch_ = batch;
  }

  ~unique_llama_batch() {
    if (batch_) {
      llama_batch_free(batch_.value());
    }
    batch_.reset();
  }

 private:
  std::optional<llama_batch> batch_;
};

}  // namespace

std::expected<GenerationResult, std::string> DefaultLlamaContext::generate(const std::string& prompt, const std::vector<std::vector<std::byte>>& files,
      std::function<void(std::string_view/*token*/)> token_handler) {
  GenerationResult result{};
  auto start_time = std::chrono::steady_clock::now();
  llama_memory_seq_rm(llama_get_memory(llama_ctx_), 0, -1, -1);
  const llama_vocab * vocab = llama_model_get_vocab(llama_model_);
  llama_pos n_past = 0;
  std::vector<llama_token> tokenized_input;
  unique_llama_batch batch;
  int32_t decode_status = 0;
  if (multimodal_ctx_) {
    gsl_Assert(!files.empty());
    std::vector<unique_bitmap_ptr> bitmaps;
    for (auto& file : files) {
      unique_bitmap_ptr bitmap{mtmd_helper_bitmap_init_from_buf(multimodal_ctx_, reinterpret_cast<const unsigned char*>(file.data()), file.size())};
      if (!bitmap) {
        throw Exception(PROCESSOR_EXCEPTION, "Failed to create multimodal bitmap from buffer");
      }
      bitmaps.push_back(std::move(bitmap));
    }
    mtmd_input_text inp_txt = {
      .text = prompt.c_str(),
      .add_special = true,
      .parse_special = true,
    };
    unique_mtmd_input_chunks_ptr chunks{mtmd_input_chunks_init()};
    auto bitmap_c_ptrs = bitmaps | std::views::transform([] (auto& ptr) {return static_cast<const mtmd_bitmap*>(ptr.get());}) | std::ranges::to<std::vector>();
    auto tokenized = mtmd_tokenize(multimodal_ctx_, chunks.get(), &inp_txt, bitmap_c_ptrs.data(), bitmap_c_ptrs.size());
    if (tokenized != 0) {
      throw Exception(PROCESSOR_EXCEPTION, fmt::format("Failed to tokenize multimodal prompt, error: {}", tokenized));
    }
    auto status = mtmd_helper_eval_chunks(multimodal_ctx_, llama_ctx_, chunks.get(), 0, 0, 1, true, &n_past);
    if (status != 0) {
      throw Exception(PROCESSOR_EXCEPTION, fmt::format("Failed to eval multimodal chunks, error: {}", status));
    }
  } else {
    gsl_Assert(files.empty());
    tokenized_input = tokenizeInput(vocab, prompt);
    n_past = gsl::narrow<llama_pos>(tokenized_input.size());
    result.num_tokens_in = gsl::narrow<uint64_t>(tokenized_input.size());
    decode_status = llama_decode(llama_ctx_, llama_batch_get_one(tokenized_input.data(), n_past));
  }

  llama_token new_token_id = 0;
  bool first_token_generated = false;
  while (decode_status == 0) {
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
      return std::unexpected{"Failed to convert token to text"};
    }
    gsl_Assert(len < 128);

    std::string_view token_str{buf.data(), gsl::narrow<std::string_view::size_type>(len)};
    batch.reset(llama_batch_init(1, 0, 1));
    batch->n_tokens = 1;
    batch->token[0] = new_token_id;
    batch->pos[0] = n_past;
    batch->n_seq_id[0] = 1;
    batch->seq_id[0][0] = 0;
    batch->logits[0] = true;
    ++n_past;
    token_handler(token_str);

    decode_status = llama_decode(llama_ctx_, batch.get().value());
  }

  if (decode_status == 1) {
    return std::unexpected("Could not find a KV slot for the batch (try reducing the size of the batch or increase the context)");
  }
  if (decode_status == 2) {
    return std::unexpected("Llama decode aborted");
  }
  if (decode_status < 0) {
    return std::unexpected("Error occurred while executing llama decode");
  }

  result.tokens_per_second =
    gsl::narrow<double>(result.num_tokens_out) / (gsl::narrow<double>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count()) / 1000.0);
  return result;
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
