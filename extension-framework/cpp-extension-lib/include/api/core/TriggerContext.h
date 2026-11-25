// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <map>
#include <span>

#include "FlowFile.h"
#include "PropertyContext.h"
#include "PublishedMetrics.h"
#include "minifi-api.h"
#include "minifi-cpp/core/Relationship.h"
#include "minifi-cpp/io/StreamCallback.h"

namespace org::apache::nifi::minifi::api::core {

class TriggerContext : public PropertyContext {
 public:
  virtual ~TriggerContext() = default;

  TriggerContext() = default;

  TriggerContext(const TriggerContext&) = delete;
  TriggerContext(TriggerContext&&) = delete;
  TriggerContext& operator=(const TriggerContext&) = delete;
  TriggerContext& operator=(TriggerContext&&) = delete;

  using PropertyContext::getProperty;
  using PropertyContext::getDynamicProperties;
  [[nodiscard]] virtual std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& prop,
      const FlowFile& flow_file) const = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> getDynamicProperties(const FlowFile& flow_file) const = 0;
  [[nodiscard]] virtual std::expected<void, std::error_code> reportMetrics(const PublishedMetrics& metrics) const = 0;

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

class CffiTriggerContext : public TriggerContext {
 public:
  CffiTriggerContext(minifi_process_context* context, minifi_process_session* impl) : context_(context), impl_(impl) {}

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const minifi::core::PropertyReference& prop,
      const FlowFile& flow_file) const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties() const override;
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties(const FlowFile& flow_file) const override;
  [[nodiscard]] std::expected<minifi_controller_service*, std::error_code> getControllerService(const minifi::core::PropertyReference& prop) const override;
  [[nodiscard]] std::expected<void, std::error_code> reportMetrics(const PublishedMetrics& metrics) const override;

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
  minifi_process_context* context_;
  minifi_process_session* impl_;
};

}  // namespace org::apache::nifi::minifi::api::core
