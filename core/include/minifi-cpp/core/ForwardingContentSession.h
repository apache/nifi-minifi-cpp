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

#include <unordered_set>
#include <memory>
#include "ResourceClaim.h"
#include "io/BaseStream.h"
#include "ContentSession.h"

namespace org::apache::nifi::minifi::core {

class ContentRepository;

/**
 * Warning: this implementation simply forwards all calls to the underlying
 * repository without any atomicity guarantees or possibility of rollback.
 */
class ForwardingContentSession : public ContentSession {
 public:
  explicit ForwardingContentSession(std::shared_ptr<ContentRepository> repository);

  std::shared_ptr<ResourceClaim> create() override;

  std::shared_ptr<io::BaseStream> write(const std::shared_ptr<ResourceClaim>& resource_id) override;

  std::shared_ptr<io::BaseStream> read(const std::shared_ptr<ResourceClaim>& resource_id) override;

  void commit() override;

  void rollback() override;

 protected:
  std::shared_ptr<io::BaseStream> append(const std::shared_ptr<ResourceClaim>& resource_id) override;

  std::unordered_set<std::shared_ptr<ResourceClaim>> created_claims_;
};

}  // namespace org::apache::nifi::minifi::core

