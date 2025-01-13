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

#include "LlamaCppProcessor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Resource.h"
#include "Exception.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "LlamaContext.h"

namespace org::apache::nifi::minifi::processors {

void LlamaCppProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void LlamaCppProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
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

  llama_ctx_ = llamacpp::LlamaContext::create(model_name_, gsl::narrow_cast<float>(temperature_));
}

void LlamaCppProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto input_ff = session.get();
  if (!input_ff) {
    context.yield();
    return;
  }
  auto ff_guard = gsl::finally([&] {
    session.remove(input_ff);
  });

  auto read_result = session.readBuffer(input_ff);
  std::string input_content{reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()};

  std::string msg;
  msg += "attributes:\n";
  for (auto& [attr_name, attr_val] : input_ff->getAttributes()) {
    msg += "  " + attr_name + ": " + attr_val + "\n";
  }
  msg += "content:\n  " + input_content + "\n";


  std::string input = [&] {
    std::vector<llamacpp::LlamaChatMessage> msgs;
    msgs.push_back({.role = "system", .content = full_prompt_.c_str()});
    for (auto& ex : examples_) {
      msgs.push_back({.role = "user", .content = ex.input.c_str()});
      msgs.push_back({.role = "assistant", .content = ex.output.c_str()});
    }
    msgs.push_back({.role = "user", .content = msg.c_str()});

    return llama_ctx_->applyTemplate(msgs);
  }();

  logger_->log_debug("AI model input: {}", input);

  std::string text;
  llama_ctx_->generate(input, [&] (std::string_view token) {
    text += token;
    return true;
  });

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

void LlamaCppProcessor::notifyStop() {
  llama_ctx_.reset();
}

REGISTER_RESOURCE(LlamaCppProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
