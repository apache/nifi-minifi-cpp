/**
 * @file FlowFileRecord.h
 * Flow file record class declaration
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

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include "core/ContentRepository.h"
#include "core/FlowFile.h"
#include "utils/TimeUtil.h"
#include "core/logging/LoggerFactory.h"
#include "ResourceClaim.h"
#include "Connection.h"
#include "io/OutputStream.h"
#include "io/StreamPipe.h"
#include "minifi-cpp/FlowFileRecord.h"

namespace org::apache::nifi::minifi {

#define DEFAULT_FLOWFILE_PATH "."

namespace core {
class ProcessSession;
}

class FlowFileRecordImpl : public core::FlowFileImpl, public virtual FlowFileRecord {
  friend class core::ProcessSession;

 public:
  FlowFileRecordImpl();

  bool Serialize(io::OutputStream &outStream) override;

  //! Serialize and Persistent to the repository
  bool Persist(const std::shared_ptr<core::Repository>& flowRepository) override;

  static std::shared_ptr<FlowFileRecord> DeSerialize(std::span<const std::byte> buffer, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container) {
    io::BufferStream inStream{buffer};
    return DeSerialize(inStream, content_repo, container);
  }
  static std::shared_ptr<FlowFileRecord> DeSerialize(io::InputStream &stream, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container);
  static std::shared_ptr<FlowFileRecord> DeSerialize(const std::string& key, const std::shared_ptr<core::Repository>& flowRepository,
                                                     const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container);

  std::string getContentFullPath() const override {
    return claim_ ? claim_->getContentFullPath() : "";
  }

 protected:
  // Local flow sequence ID
  static std::atomic<uint64_t> local_flow_seq_number_;

 private:
  static std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi
