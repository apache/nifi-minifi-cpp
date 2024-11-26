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

#include "AiProcessor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Resource.h"
#include "Exception.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

namespace org::apache::nifi::minifi::processors {

namespace {

struct LlamaChatMessage {
  std::string role;
  std::string content;

  operator llama_chat_message() const {
    return llama_chat_message{
      .role = role.c_str(),
      .content = content.c_str()
    };
  }
};

//constexpr const char* relationship_prompt = R"(You are a helpful assistant helping to analyze the user's description of a data transformation and routing algorithm.
//The data consists of attributes and a content encapsulated in what is called a flowfile.
//The routing targets are called relationships.
//You have to extract the comma separated list of all possible relationships one can route to based on the user's description.
//Output only the list and nothing else.
//)";

}  // namespace

void AiProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void AiProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  context.getProperty(ModelName, model_name_);
  context.getProperty(SystemPrompt, system_prompt_);
  context.getProperty(Prompt, prompt_);
  context.getProperty(Temperature, temperature_);
  full_prompt_ = system_prompt_ + prompt_;

  examples_.clear();
  std::string examples_str;
  context.getProperty(Examples, examples_str);
  rapidjson::Document doc;
  rapidjson::ParseResult res = doc.Parse(examples_str.data(), examples_str.length());
  if (!res) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed transformation example: {}", rapidjson::GetParseError_En(res.Code()), res.Offset()));
  }
  if (!doc.IsArray()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Malformed json example, expected array at /");
  }
  const auto& example_prop_keys = context.getDynamicPropertyKeys();
  for (rapidjson::SizeType example_idx = 0; example_idx < doc.Size(); ++example_idx) {
    auto& example = doc[example_idx];
    if (!example.IsObject()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}", example_idx));
    }
    if (!example.HasMember("input") || !example["input"].IsObject()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/input", example_idx));
    }
    if (!example.HasMember("outputs") || !example["outputs"].IsArray()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected array at /{}/outputs", example_idx));
    }
    if (!example["input"].HasMember("attributes") || !example["input"]["attributes"].IsObject()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/input/attributes", example_idx));
    }
    std::string input;
    input += "attributes:\n";
    for (auto& [attr_name, attr_val] : example["input"]["attributes"].GetObject()) {
      std::string attr_name_str{attr_name.GetString(), attr_name.GetStringLength()};
      if (!attr_val.IsString()) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected string at /{}/input/attributes/{}", example_idx, attr_name_str));
      }
      input += "  " + attr_name_str + ": " + std::string{attr_val.GetString(), attr_val.GetStringLength()} + "\n";
    }
    if (!example["input"].HasMember("content") || !example["input"]["content"].IsString()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected string at /{}/input/content", example_idx));
    }
    input += "content:\n  " + std::string{example["input"]["content"].GetString(), example["input"]["content"].GetStringLength()} + "\n";

    std::string output;
    size_t output_index{0};
    for (auto& output_ff : example["outputs"].GetArray()) {
      if (!output_ff.IsObject()) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected object at /{}/outputs/{}", example_idx, output_index));
      }
      output += "attributes:\n";
      for (auto& [attr_name, attr_val] : output_ff["attributes"].GetObject()) {
        std::string attr_name_str{attr_name.GetString(), attr_name.GetStringLength()};
        if (!attr_val.IsString()) {
          throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected string at /{}/outputs/{}/attributes/{}", example_idx, output_index, attr_name_str));
        }
        output += "  " + attr_name_str + ": " + std::string{attr_val.GetString(), attr_val.GetStringLength()} + "\n";
      }
      if (!output_ff.HasMember("content") || !output_ff["content"].IsString()) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected string at /{}/outputs/{}/content", example_idx, output_index));
      }
      output += "content:\n  " + std::string{output_ff["content"].GetString(), output_ff["content"].GetStringLength()} + "\n";
      if (!output_ff.HasMember("relationship") || !output_ff["relationship"].IsString()) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Malformed json example, expected string at /{}/outputs/{}/relationship", example_idx, output_index));
      }
      output += "relationship:\n  " + std::string{output_ff["relationship"].GetString(), output_ff["relationship"].GetStringLength()};
      ++output_index;
    }
//    output += "<NEWLINE_CHAR>";
    examples_.push_back(LLMExample{.input = std::move(input), .output = std::move(output)});
  }

  llama_backend_init();

  llama_model_params model_params = llama_model_default_params();
  llama_model_ = llama_load_model_from_file(model_name_.c_str(), model_params);
  if (!llama_model_) {
    throw Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, fmt::format("Failed to load model from '{}'", model_name_));
  }

  llama_context_params ctx_params = llama_context_default_params();
  ctx_params.n_ctx = 0;
  llama_ctx_ = llama_new_context_with_model(llama_model_, ctx_params);

  auto sparams = llama_sampler_chain_default_params();
  llama_sampler_ = llama_sampler_chain_init(sparams);

  llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_k(50));
  llama_sampler_chain_add(llama_sampler_, llama_sampler_init_top_p(0.9, 1));
  llama_sampler_chain_add(llama_sampler_, llama_sampler_init_temp(gsl::narrow_cast<float>(temperature_)));
  llama_sampler_chain_add(llama_sampler_, llama_sampler_init_dist(1234));
}

void AiProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto input_ff = session.get();
  if (!input_ff) {
    context.yield();
    return;
  }

  auto read_result = session.readBuffer(input_ff);
  std::string input_content{reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()};

  std::string msg;
  msg += "attributes:\n";
  for (auto& [attr_name, attr_val] : input_ff->getAttributes()) {
    msg += "  " + attr_name + ": " + attr_val + "\n";
  }
  msg += "content:\n  " + input_content + "\n";


  std::string input = [&] {
    std::vector<llama_chat_message> msgs;
    msgs.push_back(llama_chat_message{.role = "system", .content = full_prompt_.c_str()});
    for (auto& ex : examples_) {
      msgs.push_back(llama_chat_message{.role = "user", .content = ex.input.c_str()});
      msgs.push_back(llama_chat_message{.role = "assistant", .content = ex.output.c_str()});
    }
    msgs.push_back(llama_chat_message{.role = "user", .content = msg.c_str()});

    std::string text;
    int32_t res_size = llama_chat_apply_template(llama_model_, nullptr, msgs.data(), msgs.size(), true, text.data(), text.size());
    if (res_size > gsl::narrow<int32_t>(text.size())) {
      text.resize(res_size);
      llama_chat_apply_template(llama_model_, nullptr, msgs.data(), msgs.size(), true, text.data(), text.size());
    }
    text.resize(res_size);

//    utils::string::replaceAll(text, "<NEWLINE_CHAR>", "\n");

    return text;
  }();

  logger_->log_debug("AI model input: {}", input);

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

  std::string text;

  while (true) {
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
    std::cout << token_str << std::flush;
    text += token_str;

    batch = llama_batch_get_one(&new_token_id, 1);
  }

  logger_->log_debug("AI model output: {}", text);

  std::string_view output = text;

  while (!output.empty()) {
    auto result = session.create();
    auto rest = output;
    if (!output.starts_with("attributes:\n")) {
      // no attributes tag
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    output = output.substr(std::strlen("attributes:\n"));
    while (output.starts_with("  ")) {
      output = output.substr(std::strlen("  "));
      auto name_end = output.find(": ");
      if (name_end == std::string_view::npos) {
        // failed to parse attribute name, dump the rest as malformed
        session.writeBuffer(result, rest);
        session.transfer(result, Malformed);
        return;
      }
      auto name = output.substr(0, name_end);
      output = output.substr(name_end + std::strlen(": "));
      auto val_end = output.find("\n");
      if (val_end == std::string_view::npos) {
        // failed to parse attribute value, dump the rest as malformed
        session.writeBuffer(result, rest);
        session.transfer(result, Malformed);
        return;
      }
      auto val = output.substr(0, val_end);
      output = output.substr(val_end + std::strlen("\n"));
      result->setAttribute(name, std::string{val});
    }
    if (!output.starts_with("content:\n  ")) {
      // no content
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    output = output.substr(std::strlen("content:\n  "));
    auto content_end = output.find("\n");
    if (content_end == std::string_view::npos) {
      // no content closing tag
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    auto content = output.substr(0, content_end);
    output = output.substr(content_end + std::strlen("\n"));
    if (!output.starts_with("relationship:\n  ")) {
      // no relationship opening tag
      session.writeBuffer(result, rest);
      session.transfer(result, Malformed);
      return;
    }
    output = output.substr(std::strlen("relationship:\n  "));
    auto rel_end = output.find("\n");
    auto rel = output.substr(0, rel_end);

    session.writeBuffer(result, content);
    session.transfer(result, core::Relationship{std::string{rel}, ""});

    if (rel_end == std::string_view::npos) {
      break;
    }
    output = output.substr(rel_end + std::strlen("\n"));
  }
}

void AiProcessor::notifyStop() {
  llama_sampler_free(llama_sampler_);
  llama_sampler_ = nullptr;
  llama_free(llama_ctx_);
  llama_ctx_ = nullptr;
  llama_free_model(llama_model_);
  llama_model_ = nullptr;
  llama_backend_free();
}

REGISTER_RESOURCE(AiProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
