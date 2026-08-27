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
#include <string>
#include <vector>
#include <system_error>
#include <string_view>
#include "utils/expected.h"
#include "utils/Id.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/StateManager.h"
#include "minifi-cpp/core/controller/ControllerServiceHandle.h"
#include "minifi-cpp/provenance/ProvenanceRepository.h"

namespace org::apache::nifi::minifi::core::reporting {

// Read-only snapshot of the current flow's structure, exposed to reporting tasks so
// they can describe the flow's topology to external systems (lineage/metadata catalogs,
// monitoring back-ends, etc.). Nested process groups are flattened; only the root
// group's ports are included. Endpoints on Connection are UUIDs of processors or ports.
struct FlowTopology {
  struct Processor {
    utils::Identifier uuid;
    std::string name;
    std::string type;   // short class name (e.g. "PutS3Object"), matches how the flow config references it
  };
  struct Port {
    utils::Identifier uuid;
    std::string name;
    bool is_input;  // false => output port
  };
  struct Connection {
    utils::Identifier uuid;
    std::string name;
    utils::Identifier source_uuid;
    utils::Identifier destination_uuid;
    std::vector<std::string> relationships;
  };

  utils::Identifier root_group_uuid;
  std::string root_group_name;
  std::vector<Processor> processors;
  std::vector<Port> input_ports;
  std::vector<Port> output_ports;
  std::vector<Connection> connections;
};

class ReportingTaskContext {
 public:
  virtual ~ReportingTaskContext() = default;

  [[nodiscard]]
  virtual std::expected<std::string, std::error_code> getProperty(std::string_view name) const = 0;

  [[nodiscard]]
  std::expected<std::string, std::error_code> getProperty(const Property& property) const {
    return getProperty(property.getName());
  }

  [[nodiscard]]
  std::expected<std::string, std::error_code> getProperty(const PropertyReference& property_reference) const {
    return getProperty(property_reference.name);
  }

  [[nodiscard]]
  virtual std::shared_ptr<core::controller::ControllerServiceHandle> getControllerService(const std::string& service_name, const utils::Identifier& reporting_task_uuid) const = 0;

  [[nodiscard]]
  virtual std::shared_ptr<provenance::ProvenanceRepository> getProvenanceRepository() = 0;

  [[nodiscard]]
  virtual StateManager* getStateManager() = 0;

  [[nodiscard]]
  virtual uint8_t getMaxConcurrentTasks() const = 0;

  // Returns a snapshot of the current flow structure. The snapshot is a value type
  // owned by the caller; the reporting task may hold on to it across trigger calls
  // but must ask for a fresh one to see flow changes.
  [[nodiscard]]
  virtual FlowTopology getFlowTopology() const = 0;

  virtual void yield() = 0;
};

}  // namespace org::apache::nifi::minifi::core::reporting
