/**
 *
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
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#include "utils/TimeUtil.h"
#include "minifi-cpp/ResourceClaim.h"
#include "Connectable.h"
#include "WeakReference.h"
#include "utils/FlatMap.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::core {

class Connectable;

class FlowFile : public CoreComponent, public ReferenceContainer {
 public:
  using AttributeMap = utils::FlatMap<std::string, std::string>;

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
  [[nodiscard]] virtual std::chrono::system_clock::time_point getlineageStartDate() const = 0;
  virtual void setLineageStartDate(std::chrono::system_clock::time_point date) = 0;
  virtual void setLineageIdentifiers(const std::vector<utils::Identifier>& lineage_Identifiers) = 0;
  virtual bool getAttribute(std::string_view key, std::string& value) const = 0;
  [[nodiscard]] virtual std::optional<std::string> getAttribute(std::string_view key) const = 0;
  virtual bool updateAttribute(std::string_view key, const std::string& value) = 0;
  virtual bool removeAttribute(std::string_view key) = 0;
  virtual bool setAttribute(std::string_view key, std::string value) = 0;
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

// FlowFile Attribute
struct SpecialFlowAttribute {
  // The flowfile's path indicates the relative directory to which a FlowFile belongs and does not contain the filename
  MINIFIAPI static constexpr std::string_view PATH = "path";
  // The flowfile's absolute path indicates the absolute directory to which a FlowFile belongs and does not contain the filename
  MINIFIAPI static constexpr std::string_view ABSOLUTE_PATH = "absolute.path";
  // The filename of the FlowFile. The filename should not contain any directory structure.
  MINIFIAPI static constexpr std::string_view FILENAME = "filename";
  // A unique UUID assigned to this FlowFile.
  MINIFIAPI static constexpr std::string_view UUID = "uuid";
  // A numeric value indicating the FlowFile priority
  MINIFIAPI static constexpr std::string_view priority = "priority";
  // The MIME Type of this FlowFile
  MINIFIAPI static constexpr std::string_view MIME_TYPE = "mime.type";
  // Specifies the reason that a FlowFile is being discarded
  MINIFIAPI static constexpr std::string_view DISCARD_REASON = "discard.reason";
  // Indicates an identifier other than the FlowFile's UUID that is known to refer to this FlowFile.
  MINIFIAPI static constexpr std::string_view ALTERNATE_IDENTIFIER = "alternate.identifier";
  // Flow identifier
  MINIFIAPI static constexpr std::string_view FLOW_ID = "flow.id";

  static constexpr std::array<std::string_view, 9> getSpecialFlowAttributes() {
    return {
        PATH, ABSOLUTE_PATH, FILENAME, UUID, priority, MIME_TYPE, DISCARD_REASON, ALTERNATE_IDENTIFIER, FLOW_ID
    };
  }
};

}  // namespace org::apache::nifi::minifi::core
