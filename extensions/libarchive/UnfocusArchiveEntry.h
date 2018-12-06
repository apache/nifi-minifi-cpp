/**
 * @file UnfocusArchiveEntry.h
 * UnfocusArchiveEntry class declaration
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
#ifndef LIBMINIFI_INCLUDE_PROCESSORS_UNFOCUSARCHIVEENTRY_H_
#define LIBMINIFI_INCLUDE_PROCESSORS_UNFOCUSARCHIVEENTRY_H_

#include <memory>
#include <string>

#include <archive.h>

#include "FocusArchiveEntry.h"
#include "FlowFileRecord.h"
#include "ArchiveMetadata.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

using logging::Logger;

//! UnfocusArchiveEntry Class
class UnfocusArchiveEntry : public core::Processor {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  explicit UnfocusArchiveEntry(std::string name, utils::Identifier uuid = utils::Identifier())
  : core::Processor(name, uuid),
    logger_(logging::LoggerFactory<UnfocusArchiveEntry>::getLogger()){
  }
  //! Destructor
  virtual ~UnfocusArchiveEntry() {
  }
  //! Processor Name
  static constexpr char const* ProcessorName = "UnfocusArchiveEntry";
  //! Supported Relationships
  static core::Relationship Success;

  bool set_or_update_attr(std::shared_ptr<core::FlowFile>, const std::string&, const std::string&) const;

  //! OnTrigger method, implemented by NiFi UnfocusArchiveEntry
  virtual void onTrigger(core::ProcessContext *context,
      core::ProcessSession *session);
  //! Initialize, over write by NiFi UnfocusArchiveEntry
  virtual void initialize(void);

  //! Write callback for reconstituting lensed archive into flow file content
  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(ArchiveMetadata *archiveMetadata);
    int64_t process(std::shared_ptr<io::BaseStream> stream);
   private:
    //! Logger
    std::shared_ptr<Logger> logger_;
    ArchiveMetadata *_archiveMetadata;
    static int ok_cb(struct archive *, void *d) { return ARCHIVE_OK; }
    static ssize_t write_cb(struct archive *, void *d, const void *buffer, size_t length);
  };

 private:
  //! Logger
  std::shared_ptr<Logger> logger_;
};

REGISTER_RESOURCE(UnfocusArchiveEntry, "Restores a FlowFile which has had an archive entry focused via FocusArchiveEntry to its original state.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_PROCESSORS_UNFOCUSARCHIVEENTRY_H_
