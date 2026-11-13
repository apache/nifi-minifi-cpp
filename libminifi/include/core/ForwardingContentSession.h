// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <unordered_set>
#include <memory>
#include "minifi-cpp/ResourceClaim.h"
#include "minifi-cpp/io/BaseStream.h"
#include "minifi-cpp/core/ContentRepository.h"
#include "core/ContentSession.h"

namespace org::apache::nifi::minifi::core {

class ContentRepository;

/**
 * Warning: this implementation simply forwards all calls to the underlying
 * repository without any atomicity guarantees or possibility of rollback.
 */
class ForwardingContentSession : public ContentSessionImpl {
 public:
  explicit ForwardingContentSession(std::shared_ptr<ContentRepository> repository);

  std::shared_ptr<ResourceClaim> create() override;

  std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resource_id) override;

  std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resource_id) override;

  void remove(const std::shared_ptr<ResourceClaim>& resource_id) override;

  void commit() override;

  void rollback() override;

 protected:
  std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id) override;

  std::unordered_set<std::shared_ptr<ResourceClaim>> created_claims_;
};

}  // namespace org::apache::nifi::minifi::core

