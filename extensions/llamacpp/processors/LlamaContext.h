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

#include <memory>
#include <filesystem>
#include <vector>
#include <string_view>
#include <string>
#include <functional>

namespace org::apache::nifi::minifi::processors::llamacpp {

struct LlamaChatMessage {
  std::string role;
  std::string content;
};

class LlamaContext {
 public:
  static void testSetProvider(std::function<std::unique_ptr<LlamaContext>(const std::filesystem::path&, float)> provider);
  static std::unique_ptr<LlamaContext> create(const std::filesystem::path& model_path, float temperature);
  virtual std::string applyTemplate(const std::vector<LlamaChatMessage>& messages) = 0;
  virtual void generate(const std::string& input, std::function<bool(std::string_view/*token*/)> cb) = 0;
  virtual ~LlamaContext() = default;
};

}  // namespace org::apache::nifi::minifi::processors::llamacpp
