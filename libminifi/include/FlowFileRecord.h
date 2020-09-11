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
#ifndef LIBMINIFI_INCLUDE_FLOWFILERECORD_H_
#define LIBMINIFI_INCLUDE_FLOWFILERECORD_H_

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
#include "io/BaseStream.h"
#include "core/FlowFile.h"
#include "utils/TimeUtil.h"
#include "core/logging/LoggerConfiguration.h"
#include "ResourceClaim.h"
#include "Connection.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

#define DEFAULT_FLOWFILE_PATH "."

// FlowFile IO Callback functions for input and output
// throw exception for error
class InputStreamCallback {
 public:
  virtual ~InputStreamCallback() = default;
  // virtual void process(std::ifstream *stream) = 0;

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) = 0;
};
class OutputStreamCallback {
 public:
  virtual ~OutputStreamCallback() = default;
  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) = 0;
};

namespace core {
class ProcessSession;
}

class FlowFileRecord : public core::FlowFile, public io::Serializable {
  friend class core::ProcessSession;

 public:
  bool Serialize(io::BufferStream &outStream);

  //! Serialize and Persistent to the repository
  bool Persist(const std::shared_ptr<core::Repository>& flowRepository);
  //! DeSerialize
  static std::shared_ptr<FlowFileRecord> DeSerialize(const uint8_t *buffer, int bufferSize, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier& container);
  //! DeSerialize
  static std::shared_ptr<FlowFileRecord> DeSerialize(io::BufferStream &stream, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier& container) {
    return DeSerialize(stream.getBuffer(), gsl::narrow<int>(stream.size()), content_repo, container);
  }
  //! DeSerialize
  static std::shared_ptr<FlowFileRecord> DeSerialize(const std::string& key, const std::shared_ptr<core::Repository>& flowRepository,
      const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier& container);

  std::string getContentFullPath() {
    return claim_ ? claim_->getContentFullPath() : "";
  }

 protected:
  // Local flow sequence ID
  static std::atomic<uint64_t> local_flow_seq_number_;

 private:
  static std::shared_ptr<logging::Logger> logger_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_FLOWFILERECORD_H_
