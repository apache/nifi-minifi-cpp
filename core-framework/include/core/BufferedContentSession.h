// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <map>
#include "minifi-cpp/io/BaseStream.h"
#include "ContentSession.h"

namespace org::apache::nifi::minifi::core {

class ContentRepository;

/**
 * Buffers the changes in-memory and forwards those to the repository on commit.
 * Atomicity is NOT guaranteed in this implementation, override commit if needed.
 * Rollback is possible.
 */
class BufferedContentSession : public ContentSessionImpl {
 public:
  explicit BufferedContentSession(std::shared_ptr<ContentRepository> repository);

  std::shared_ptr<ResourceClaim> create() override;

  std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resource_id) override;

  std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id, size_t offset, const std::function<void(const std::shared_ptr<ResourceClaim>&)>& on_copy) override;

  std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resource_id) override;

  void remove(const std::shared_ptr<ResourceClaim>& resource_id) override;

  void commit() override;

  void rollback() override;

 protected:
  std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id) override;

  std::map<std::shared_ptr<ResourceClaim>, std::shared_ptr<io::BufferStream>> managed_resources_;
};

}  // namespace org::apache::nifi::minifi::core

