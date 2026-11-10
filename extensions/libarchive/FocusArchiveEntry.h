// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "archive.h"

#include "ArchiveMetadata.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "utils/file/FileManager.h"
#include "minifi-cpp/utils/Export.h"

namespace org::apache::nifi::minifi::processors {

class FocusArchiveEntry : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;
  ~FocusArchiveEntry()   override = default;

  EXTENSIONAPI static constexpr const char* Description = "Allows manipulation of entries within an archive (e.g. TAR) by focusing on one entry within the archive at a time. "
      "When an archive entry is focused, that entry is treated as the content of the FlowFile and may be manipulated independently of the rest of the archive. "
      "To restore the FlowFile to its original state, use UnfocusArchiveEntry.";

  EXTENSIONAPI static constexpr auto Path = core::PropertyDefinitionBuilder<>::createProperty("Path")
      .withDescription("The path within the archive to focus (\"/\" to focus the total archive)")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({Path});

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  class ReadCallback {
   public:
    explicit ReadCallback(core::ProcessContext* context, utils::file::FileManager *file_man, ArchiveMetadata *archiveMetadata);
    io::IoResult operator()(const std::shared_ptr<io::InputStream>& stream) const;

   private:
    utils::file::FileManager *file_man_;
    core::ProcessContext* const context_;
    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FocusArchiveEntry>::getLogger();
    ArchiveMetadata *_archiveMetadata;
    static int ok_cb(struct archive *, void* /*d*/) { return ARCHIVE_OK; }
    static la_ssize_t read_cb(struct archive * a, void *d, const void **buf);
  };

 private:
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

}  // namespace org::apache::nifi::minifi::processors
