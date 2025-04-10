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
#include "DefaultLlamaContext.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

static std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, const LlamaSamplerParams&, const LlamaContextParams&)> test_provider;

void LlamaContext::testSetProvider(std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, const LlamaSamplerParams&, const LlamaContextParams&)> provider) {
  test_provider = std::move(provider);
}

std::unique_ptr<LlamaContext> LlamaContext::create(const std::filesystem::path& model_path, const LlamaSamplerParams& llama_sampler_params, const LlamaContextParams& llama_ctx_params) {
  if (test_provider) {
    return test_provider(model_path, llama_sampler_params, llama_ctx_params);
  }
  return std::make_unique<DefaultLlamaContext>(model_path, llama_sampler_params, llama_ctx_params);
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
