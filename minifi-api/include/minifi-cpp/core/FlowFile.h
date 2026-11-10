// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "minifi-cpp/ResourceClaim.h"
#include "minifi-cpp/core/Connectable.h"
#include "WeakReference.h"
#include "minifi-cpp/utils/FlatMap.h"
#include "minifi-cpp/core/SpecialFlowAttribute.h"

namespace org::apache::nifi::minifi::core {

class FlowFile : public virtual CoreComponent, public virtual ReferenceContainer {
 public:
  using AttributeMap = utils::FlatMap<std::string, std::string>;

  virtual void copy(const FlowFile&) = 0;

  [[nodiscard]] virtual std::shared_ptr<ResourceClaim> getResourceClaim() const = 0;
  virtual void setResourceClaim(const std::shared_ptr<ResourceClaim>& claim) = 0;
  virtual void clearResourceClaim() = 0;
  virtual std::shared_ptr<ResourceClaim> getStashClaim(const std::string& key) = 0;
  virtual void setStashClaim(const std::string& key, const std::shared_ptr<ResourceClaim>& claim) = 0;
  virtual void clearStashClaim(const std::string& key) = 0;
  virtual bool hasStashClaim(const std::string& key) = 0;
  virtual const std::vector<utils::Identifier>& getlineageIdentifiers() const = 0;
  virtual std::vector<utils::Identifier>& getlineageIdentifiers() = 0;
  [[nodiscard]] virtual bool isDeleted() const = 0;
  virtual void setDeleted(bool deleted) = 0;
  [[nodiscard]] virtual std::chrono::system_clock::time_point getEntryDate() const = 0;
  [[nodiscard]] virtual std::chrono::system_clock::time_point getEventTime() const = 0;
  [[nodiscard]] virtual std::chrono::system_clock::time_point getLineageStartDate() const = 0;
  virtual void setLineageStartDate(std::chrono::system_clock::time_point date) = 0;
  virtual void setLineageIdentifiers(const std::vector<utils::Identifier>& lineage_Identifiers) = 0;
  virtual void setAttribute(std::string_view name, std::string value) = 0;
  virtual bool getAttribute(std::string_view key, std::string& value) const = 0;
  [[nodiscard]] virtual std::optional<std::string> getAttribute(std::string_view key) const = 0;
  virtual bool updateAttribute(std::string_view key, const std::string& value) = 0;
  virtual bool removeAttribute(std::string_view key) = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> getAttributes() const = 0;
  virtual AttributeMap *getAttributesPtr() = 0;
  virtual bool addAttribute(std::string_view key, const std::string& value) = 0;
  virtual void setSize(const uint64_t size) = 0;
  [[nodiscard]] virtual uint64_t getSize() const = 0;
  virtual void setOffset(const uint64_t offset) = 0;
  [[nodiscard]] virtual std::chrono::steady_clock::time_point getPenaltyExpiration() const = 0;
  virtual void setPenaltyExpiration(std::chrono::time_point<std::chrono::steady_clock> to_be_processed_after) = 0;
  [[nodiscard]] virtual uint64_t getOffset() const = 0;
  [[nodiscard]] virtual bool isPenalized() const = 0;
  [[nodiscard]] virtual uint64_t getId() const = 0;
  virtual void setConnection(core::Connectable* connection) = 0;
  [[nodiscard]] virtual Connectable* getConnection() const = 0;
  virtual void setStoredToRepository(bool storedInRepository) = 0;
  [[nodiscard]] virtual bool isStored() const = 0;

  static std::shared_ptr<FlowFile> create();
};

}  // namespace org::apache::nifi::minifi::core
