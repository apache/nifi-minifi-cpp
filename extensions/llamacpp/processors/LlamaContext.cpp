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

#include "LlamaContext.h"
#include "Exception.h"
#include "fmt/format.h"
#pragma push_macro("DEPRECATED")
#include "llama.h"
#pragma pop_macro("DEPRECATED")

namespace org::apache::nifi::minifi::processors::llamacpp {

static std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, float)> test_provider;

void LlamaContext::testSetProvider(std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, float)> provider) {
  test_provider = provider;
}

class DefaultLlamaContext : public LlamaContext {
 public:
  DefaultLlamaContext(const std::filesystem::path& model_path, float temperature) {
    llama_backend_init();

    llama_model_params model_params = llama_model_default_params();
    llama_model_ = llama_load_model_from_file(model_path.c_str(), model_params);
    if (!llama_model_) {
      throw Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, fmt::format("Failed to load model from '{}'", model_path.c_str()));
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 0;
    llama_ctx_ = llama_new_context_with_model(llama_model_, ctx_params);

    auto sparams = llama_sampler_chain_default_params();
    llama_sampler_ = llama_sampler_chain_init(sparams);

    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_k(50));
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_p(0.9, 1));
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_temp(temperature));
    llama_sampler_chain_add(llama_sampler_, llama_sampler_init_dist(1234));
  }

  std::string applyTemplate(const std::vector<LlamaChatMessage>& messages) override {
    std::vector<llama_chat_message> msgs;
    for (auto& msg : messages) {
      msgs.push_back(llama_chat_message{.role = msg.role.c_str(), .content = msg.content.c_str()});
    }
    std::string text;
    int32_t res_size = llama_chat_apply_template(llama_model_, nullptr, msgs.data(), msgs.size(), true, text.data(), text.size());
    if (res_size > gsl::narrow<int32_t>(text.size())) {
      text.resize(res_size);
      llama_chat_apply_template(llama_model_, nullptr, msgs.data(), msgs.size(), true, text.data(), text.size());
    }
    text.resize(res_size);

//    utils::string::replaceAll(text, "<NEWLINE_CHAR>", "\n");

    return text;
  }

  void generate(const std::string& input, std::function<bool(std::string_view/*token*/)> cb) override {
    std::vector<llama_token> enc_input = [&] {
      int32_t n_tokens = input.length() + 2;
      std::vector<llama_token> enc_input(n_tokens);
      n_tokens = llama_tokenize(llama_model_, input.data(), input.length(), enc_input.data(), enc_input.size(), true, true);
      if (n_tokens < 0) {
        enc_input.resize(-n_tokens);
        int check = llama_tokenize(llama_model_, input.data(), input.length(), enc_input.data(), enc_input.size(), true, true);
        gsl_Assert(check == -n_tokens);
      } else {
        enc_input.resize(n_tokens);
      }
      return enc_input;
    }();


    llama_batch batch = llama_batch_get_one(enc_input.data(), enc_input.size());

    llama_token new_token_id;

    bool terminate = false;

    while (!terminate) {
      if (int32_t res = llama_decode(llama_ctx_, batch); res < 0) {
        throw std::logic_error("failed to execute decode");
      }

      new_token_id = llama_sampler_sample(llama_sampler_, llama_ctx_, -1);

      if (llama_token_is_eog(llama_model_, new_token_id)) {
        break;
      }

      llama_sampler_accept(llama_sampler_, new_token_id);

      std::array<char, 128> buf;
      int32_t len = llama_token_to_piece(llama_model_, new_token_id, buf.data(), buf.size(), 0, true);
      if (len < 0) {
        throw std::logic_error("failed to convert to text");
      }
      gsl_Assert(len < 128);

      std::string_view token_str{buf.data(), gsl::narrow<std::string_view::size_type>(len)};

      batch = llama_batch_get_one(&new_token_id, 1);

      terminate = cb(token_str);
    }
  }

  ~DefaultLlamaContext() override {
    llama_sampler_free(llama_sampler_);
    llama_sampler_ = nullptr;
    llama_free(llama_ctx_);
    llama_ctx_ = nullptr;
    llama_free_model(llama_model_);
    llama_model_ = nullptr;
    llama_backend_free();
  }

 private:
  llama_model* llama_model_{nullptr};
  llama_context* llama_ctx_{nullptr};
  llama_sampler* llama_sampler_{nullptr};
};

std::unique_ptr<LlamaContext> LlamaContext::create(const std::filesystem::path& model_path, float temperature) {
  if (test_provider) {
    return test_provider(model_path, temperature);
  }
  return std::make_unique<DefaultLlamaContext>(model_path, temperature);
}

}  // namespace org::apache::nifi::minifi::processors::llamacpp
