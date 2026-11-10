// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <utility>
#include <map>
#include "minifi-cpp/ResourceClaim.h"
#include "minifi-cpp/io/BaseStream.h"

namespace org::apache::nifi::minifi::core {

class StreamAppendLock;
class ContentRepository;

class ContentSession {
 public:
  virtual std::shared_ptr<ResourceClaim> create() = 0;

  virtual std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resource_id) = 0;

  virtual std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id, size_t offset, const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) = 0;

  virtual std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resource_id) = 0;

  virtual void remove(const std::shared_ptr<ResourceClaim>& resource_id) = 0;

  virtual void commit() = 0;

  virtual void rollback() = 0;

  virtual ~ContentSession() = default;
};

}  // namespace org::apache::nifi::minifi::core

