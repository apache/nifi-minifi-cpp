/**
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
#include <utility>

#include "archive.h"

#include "ArchiveMetadata.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileManager.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class FocusArchiveEntry : public core::Processor {
 public:
  explicit FocusArchiveEntry(std::string name, const utils::Identifier& uuid = {})
  : core::Processor(std::move(name), uuid) {
  }
  ~FocusArchiveEntry()   override = default;

  EXTENSIONAPI static constexpr const char* Description = "Allows manipulation of entries within an archive (e.g. TAR) by focusing on one entry within the archive at a time. "
      "When an archive entry is focused, that entry is treated as the content of the FlowFile and may be manipulated independently of the rest of the archive. "
      "To restore the FlowFile to its original state, use UnfocusArchiveEntry.";

  EXTENSIONAPI static const core::Property Path;
  static auto properties() { return std::array{Path}; }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

  class ReadCallback {
   public:
    explicit ReadCallback(core::Processor*, utils::file::FileManager *file_man, ArchiveMetadata *archiveMetadata);
    int64_t operator()(const std::shared_ptr<io::InputStream>& stream) const;
    bool isRunning() {return proc_->isRunning();}

   private:
    utils::file::FileManager *file_man_;
    core::Processor * const proc_;
    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FocusArchiveEntry>::getLogger();
    ArchiveMetadata *_archiveMetadata;
    static int ok_cb(struct archive *, void* /*d*/) { return ARCHIVE_OK; }
    static la_ssize_t read_cb(struct archive * a, void *d, const void **buf);
  };

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FocusArchiveEntry>::getLogger();
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

}  // namespace org::apache::nifi::minifi::processors
