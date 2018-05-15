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
#ifndef __FLOW_FILE_RECORD_H__
#define __FLOW_FILE_RECORD_H__

#include <uuid/uuid.h>
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
#include "io/Serializable.h"
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

// FlowFile Attribute
enum FlowAttribute {
  // The flowfile's path indicates the relative directory to which a FlowFile belongs and does not contain the filename
  PATH = 0,
  // The flowfile's absolute path indicates the absolute directory to which a FlowFile belongs and does not contain the filename
  ABSOLUTE_PATH,
  // The filename of the FlowFile. The filename should not contain any directory structure.
  FILENAME,
  // A unique UUID assigned to this FlowFile.
  UUID,
  // A numeric value indicating the FlowFile priority
  priority,
  // The MIME Type of this FlowFile
  MIME_TYPE,
  // Specifies the reason that a FlowFile is being discarded
  DISCARD_REASON,
  // Indicates an identifier other than the FlowFile's UUID that is known to refer to this FlowFile.
  ALTERNATE_IDENTIFIER,
  // Flow identifier
  FLOW_ID,
  MAX_FLOW_ATTRIBUTES
};

// FlowFile Attribute Key
static const char *FlowAttributeKeyArray[MAX_FLOW_ATTRIBUTES] = { "path", "absolute.path", "filename", "uuid", "priority", "mime.type", "discard.reason", "alternate.identifier", "flow.id" };

// FlowFile Attribute Enum to Key
inline const char *FlowAttributeKey(FlowAttribute attribute) {
  if (attribute < MAX_FLOW_ATTRIBUTES)
    return FlowAttributeKeyArray[attribute];
  else
    return NULL;
}

// FlowFile IO Callback functions for input and output
// throw exception for error
class InputStreamCallback {
 public:
  virtual ~InputStreamCallback() {

  }
  //virtual void process(std::ifstream *stream) = 0;

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) = 0;
};
class OutputStreamCallback {
 public:
  virtual ~OutputStreamCallback() {

  }
  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) = 0;

};

class FlowFileRecord : public core::FlowFile, public io::Serializable {
 public:
  // Constructor
  /*
   * Create a new flow record
   */
  explicit FlowFileRecord(std::shared_ptr<core::Repository> flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::map<std::string, std::string> attributes,
                          std::shared_ptr<ResourceClaim> claim = nullptr);

  explicit FlowFileRecord(std::shared_ptr<core::Repository> flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::shared_ptr<core::FlowFile> &event);

  explicit FlowFileRecord(std::shared_ptr<core::Repository> flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::shared_ptr<core::FlowFile> &event,
                          const std::string &uuidConnection);

  explicit FlowFileRecord(std::shared_ptr<core::Repository> flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo)
      : FlowFile(),
        flow_repository_(flow_repository),
        content_repo_(content_repo),
        snapshot_("") {

  }
  // Destructor
  virtual ~FlowFileRecord();
  // addAttribute key is enum
  bool addKeyedAttribute(FlowAttribute key, const std::string &value);
  // removeAttribute key is enum
  bool removeKeyedAttribute(FlowAttribute key);
  // updateAttribute key is enum
  bool updateKeyedAttribute(FlowAttribute key, std::string value);
  // getAttribute key is enum
  bool getKeyedAttribute(FlowAttribute key, std::string &value);

  //! Serialize and Persistent to the repository
  bool Serialize();
  //! DeSerialize
  bool DeSerialize(const uint8_t *buffer, const int bufferSize);
  //! DeSerialize
  bool DeSerialize(io::DataStream &stream) {
    return DeSerialize(stream.getBuffer(), stream.getSize());
  }
  //! DeSerialize
  bool DeSerialize(std::string key);

  void setSnapShot(bool snapshot) {
    snapshot_ = snapshot;
  }

  /**
   * gets the UUID connection.
   * @return uuidConnection
   */
  const std::string getConnectionUuid() {
    return uuid_connection_;
  }

  /**
   * Set the UUID connection.
   */
  void setUuidConnection(const std::string &uuid_connection) {
    uuid_connection_ = uuid_connection;
  }

  const std::string getContentFullPath() {
    return content_full_fath_;
  }

  /**
   * Cleanly relinquish a resource claim
   */
  virtual void releaseClaim(std::shared_ptr<ResourceClaim> claim);

  FlowFileRecord &operator=(const FlowFileRecord &);

  FlowFileRecord(const FlowFileRecord &parent) = delete;

 protected:

  // connection uuid
  std::string uuid_connection_;
  // Full path to the content
  std::string content_full_fath_;

  // Local flow sequence ID
  static std::atomic<uint64_t> local_flow_seq_number_;

  // repository reference.
  std::shared_ptr<core::Repository> flow_repository_;

  // content repo reference.
  std::shared_ptr<core::ContentRepository> content_repo_;

  // Snapshot flow record for session rollback
  bool snapshot_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer

 private:
  static std::shared_ptr<logging::Logger> logger_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
