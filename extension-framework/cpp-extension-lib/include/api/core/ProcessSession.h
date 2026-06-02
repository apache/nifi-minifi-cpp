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

#include <map>
#include <span>

#include "FlowFile.h"
#include "minifi-api.h"
#include "minifi-cpp/core/Relationship.h"
#include "minifi-cpp/io/StreamCallback.h"

namespace org::apache::nifi::minifi::api::core {

class ProcessSession {
 public:
  virtual ~ProcessSession() = default;

  ProcessSession() = default;

  ProcessSession(const ProcessSession&) = delete;
  ProcessSession(ProcessSession&&) = delete;
  ProcessSession& operator=(const ProcessSession&) = delete;
  ProcessSession& operator=(ProcessSession&&) = delete;

  virtual FlowFile create(const FlowFile* parent = nullptr) = 0;
  virtual FlowFile get() = 0;

  virtual void penalize(FlowFile& ff) = 0;
  virtual void transfer(FlowFile ff, const minifi::core::Relationship& relationship) = 0;
  virtual void remove(FlowFile ff) = 0;
  virtual void write(FlowFile& flow, const io::OutputStreamCallback& callback) = 0;
  virtual void read(FlowFile& flow, const io::InputStreamCallback& callback) = 0;

  virtual void setAttribute(FlowFile& ff, std::string_view key, std::string value) = 0;
  virtual void removeAttribute(FlowFile& ff, std::string_view key) = 0;
  [[nodiscard]] virtual std::optional<std::string> getAttribute(FlowFile& ff, std::string_view key) = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> getAttributes(const FlowFile& ff) const = 0;
  [[nodiscard]] virtual std::string getFlowFileId(const FlowFile& ff) const = 0;
  [[nodiscard]] virtual uint64_t getFlowFileSize(const FlowFile& ff) const = 0;

  void writeBuffer(FlowFile& flow_file, std::span<const char> buffer);
  void writeBuffer(FlowFile& flow_file, std::span<const std::byte> buffer);
  [[nodiscard]] std::vector<std::byte> readBuffer(FlowFile& flow_file);
};

class CffiProcessSession : public ProcessSession {
 public:
  explicit CffiProcessSession(minifi_process_session* impl): impl_(impl) {}

  FlowFile create(const FlowFile* parent = nullptr) override;
  FlowFile get() override;
  void penalize(FlowFile& ff) override;
  void transfer(FlowFile ff, const minifi::core::Relationship& relationship) override;
  void remove(FlowFile ff) override;
  void write(FlowFile& flow, const io::OutputStreamCallback& callback) override;
  void read(FlowFile& flow, const io::InputStreamCallback& callback) override;

  void setAttribute(FlowFile& ff, std::string_view key, std::string value) override;
  void removeAttribute(FlowFile& ff, std::string_view key) override;
  [[nodiscard]] std::optional<std::string> getAttribute(FlowFile& ff, std::string_view key) override;
  [[nodiscard]] std::map<std::string, std::string> getAttributes(const FlowFile& ff) const override;
  [[nodiscard]] std::string getFlowFileId(const FlowFile& ff) const override;
  [[nodiscard]] uint64_t getFlowFileSize(const FlowFile& ff) const override;

 private:
  minifi_process_session* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
