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
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "archive.h"

#include "FocusArchiveEntry.h"
#include "FlowFileRecord.h"
#include "ArchiveMetadata.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::processors {

using core::logging::Logger;

class UnfocusArchiveEntry : public core::ProcessorImpl {
 public:
  explicit UnfocusArchiveEntry(std::string_view name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(name, uuid) {
  }
  ~UnfocusArchiveEntry() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Restores a FlowFile which has had an archive entry focused via FocusArchiveEntry to its original state.";

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  //! Write callback for reconstituting lensed archive into flow file content
  class WriteCallback {
   public:
    explicit WriteCallback(ArchiveMetadata *archiveMetadata);
    int64_t operator()(const std::shared_ptr<io::OutputStream>& stream) const;
   private:
    //! Logger
    std::shared_ptr<Logger> logger_ = core::logging::LoggerFactory<UnfocusArchiveEntry>::getLogger();
    ArchiveMetadata *_archiveMetadata;
    static int ok_cb(struct archive *, void* /*d*/) { return ARCHIVE_OK; }
    static la_ssize_t write_cb(struct archive *, void *d, const void *buffer, size_t length);
  };

 private:
  std::shared_ptr<Logger> logger_ = core::logging::LoggerFactory<UnfocusArchiveEntry>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
