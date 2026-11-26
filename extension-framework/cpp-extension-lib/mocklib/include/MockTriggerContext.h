// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <map>
#include <ranges>

#include "MockProperties.h"
#include "MockScheduleContext.h"
#include "MockUtils.h"
#include "api/core/TriggerContext.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"

struct minifi_flow_file {
  explicit minifi_flow_file(std::string content_str) {
    this->content = content_str
           | ranges::views::transform([](char c) { return static_cast<std::byte>(c); })
           | ranges::to<std::vector<std::byte>>();
  }

  ~minifi_flow_file() = default;

  minifi_flow_file() = default;
  minifi_flow_file(const minifi_flow_file&) = default;
  minifi_flow_file(minifi_flow_file&&) = default;
  minifi_flow_file& operator=(const minifi_flow_file&) = default;
  minifi_flow_file& operator=(minifi_flow_file&&) = default;

  std::map<std::string, std::string> attributes;
  std::vector<std::byte> content;
  bool is_penalized = false;
  std::string id;
};

namespace org::apache::nifi::minifi::mock {

class MockTriggerContext : public api::core::TriggerContext {
 public:
  MockTriggerContext() = default;
  explicit MockTriggerContext(const MockScheduleContext& schedule_context) : properties_(schedule_context.properties_) {}
  MockTriggerContext(const MockTriggerContext&) = delete;
  MockTriggerContext& operator=(const MockTriggerContext&) = delete;
  MockTriggerContext(MockTriggerContext&&) = delete;
  MockTriggerContext& operator=(MockTriggerContext&&) = delete;

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const core::PropertyReference& property_reference) const override {
    return properties_.get(property_reference);
  }
  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(const core::PropertyReference& property_reference,
      const api::core::FlowFile&) const override {
    return properties_.get(property_reference);
  }
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties() const override { return {}; }
  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties(const api::core::FlowFile&) const override { return {}; }
  [[nodiscard]] std::expected<minifi_controller_service*, std::error_code> getControllerService(const core::PropertyReference&) const override {
    return nullptr;
  }
  [[nodiscard]] std::expected<void, std::error_code> reportMetrics(const api::core::PublishedMetrics& metrics) const override {
    *last_published_metrics_ = metrics;
    return {};
  }

  MockProperties properties_;
  std::unique_ptr<api::core::PublishedMetrics> last_published_metrics_ = std::make_unique<api::core::PublishedMetrics>();

  api::core::FlowFile create(const api::core::FlowFile* parent) override;
  api::core::FlowFile get() override;
  void penalize(api::core::FlowFile& ff) override;
  void transfer(api::core::FlowFile ff, const minifi::core::Relationship& relationship) override;
  void remove(api::core::FlowFile ff) override;
  void write(api::core::FlowFile& ff, const io::OutputStreamCallback& callback) override;
  void read(api::core::FlowFile& ff, const io::InputStreamCallback& callback) override;
  void setAttribute(api::core::FlowFile& ff, std::string_view key, std::string value) override;
  void removeAttribute(api::core::FlowFile& ff, std::string_view key) override;
  std::optional<std::string> getAttribute(api::core::FlowFile& ff, std::string_view key) override;
  [[nodiscard]] std::map<std::string, std::string> getAttributes(const api::core::FlowFile& ff) const override;
  [[nodiscard]] std::string getFlowFileId(const api::core::FlowFile& ff) const override;
  [[nodiscard]] uint64_t getFlowFileSize(const api::core::FlowFile& ff) const override;

  void addInputFlowFile(std::unique_ptr<minifi_flow_file> flow_file);

 private:
  std::vector<std::unique_ptr<minifi_flow_file>> input_flow_files_;
  using RelationshipName = std::string;
  std::map<RelationshipName, std::vector<std::unique_ptr<minifi_flow_file>>> transferred_flow_files_;
  std::vector<std::unique_ptr<minifi_flow_file>> removed_flow_files_;
};

}  // namespace org::apache::nifi::minifi::mock
