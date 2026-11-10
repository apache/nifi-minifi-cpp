// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <utility>
#include <map>
#include "minifi-cpp/io/BaseStream.h"
#include "minifi-cpp/core/ContentSession.h"
#include "minifi-cpp/core/ContentRepository.h"

namespace org::apache::nifi::minifi::core {

class StreamAppendLock;

class ContentSessionImpl : public virtual ContentSession {
  struct AppendState {
    std::shared_ptr<io::BaseStream> stream;
    size_t base_size;
    std::unique_ptr<StreamAppendLock> lock;
  };

 public:
  explicit ContentSessionImpl(std::shared_ptr<ContentRepository> repository): repository_(std::move(repository)) {}

  std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id, size_t offset, const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) override;

 protected:
  virtual std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id) = 0;

  // contains aux data on resources that have been appended to
  std::map<std::shared_ptr<ResourceClaim>, AppendState> append_state_;
  std::shared_ptr<ContentRepository> repository_;
};

}  // namespace org::apache::nifi::minifi::core

