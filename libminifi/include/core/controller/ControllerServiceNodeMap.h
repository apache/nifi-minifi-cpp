// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <set>
#include <vector>
#include <map>
#include <string>
#include "ControllerServiceNode.h"
#include "io/validation.h"

namespace org::apache::nifi::minifi::core {

class ProcessGroup;

namespace controller {

class ControllerServiceNodeMap {
 public:
  ControllerServiceNodeMap() = default;
  ~ControllerServiceNodeMap() = default;
  ControllerServiceNodeMap(const ControllerServiceNodeMap&) = delete;
  ControllerServiceNodeMap& operator=(const ControllerServiceNodeMap&) = delete;
  ControllerServiceNodeMap(ControllerServiceNodeMap&&) = delete;
  ControllerServiceNodeMap& operator=(ControllerServiceNodeMap&&) = delete;

  ControllerServiceNode* get(const std::string &id) const;
  ControllerServiceNode* get(const std::string &id, const utils::Identifier &processor_or_controller_uuid) const;

  bool put(std::string id, std::shared_ptr<ControllerServiceNode> controller_service_node, ProcessGroup* parent_group);

  bool registerAlternativeKey(std::string primary_key, std::string alternative_key);

  void clear();
  std::vector<std::shared_ptr<ControllerServiceNode>> getAllControllerServices() const;

 protected:
  mutable std::mutex mutex_;

  struct ServiceEntry {
    std::shared_ptr<ControllerServiceNode> controller_service_node;
    ProcessGroup* parent_group;
  };

  const ServiceEntry* getEntry(std::string_view primary_key, const std::scoped_lock<std::mutex>& mutex) const;

  std::map<std::string, ServiceEntry, std::less<>> services_;
  std::map<std::string, std::string, std::less<>> alternative_keys_;
};

}  // namespace controller
}  // namespace org::apache::nifi::minifi::core
