// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "minifi-cpp/core/Connectable.h"
#include "ContentRepository.h"
#include "SerializableComponent.h"
#include "RepositoryMetricsSource.h"
#include "minifi-cpp/properties/Configure.h"
#include "core/Core.h"

namespace org::apache::nifi::minifi::core {

class Repository : public virtual core::CoreComponent, public virtual core::RepositoryMetricsSource {
 public:
  using EntryStreams = std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>>;

  virtual bool initialize(const std::shared_ptr<Configure> &configure) = 0;
  virtual bool start() = 0;
  virtual bool stop() = 0;
  virtual bool isNoop() const = 0;
  virtual void flush() = 0;

  virtual bool Put(const std::string& /*key*/, const uint8_t* /*buf*/, size_t /*bufLen*/) = 0;
  virtual bool MultiPut(const EntryStreams& /*data*/) = 0;
  virtual bool Delete(const std::string& /*key*/) = 0;
  virtual bool Delete(const std::shared_ptr<core::CoreComponent>& item) = 0;
  virtual bool Delete(std::vector<std::shared_ptr<core::SerializableComponent>> &storedValues) = 0;

  virtual void setConnectionMap(std::map<std::string, core::Connectable*> connectionMap) = 0;

  virtual void setContainers(std::map<std::string, core::Connectable*> containers) = 0;

  virtual bool Get(const std::string& /*key*/, std::string& /*value*/) = 0;

  virtual void loadComponent(const std::shared_ptr<core::ContentRepository>& /*content_repo*/) = 0;

  virtual std::string getDirectory() const = 0;
};

}  // namespace org::apache::nifi::minifi::core
