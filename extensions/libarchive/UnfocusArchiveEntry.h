// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "archive.h"

#include "FocusArchiveEntry.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "ArchiveMetadata.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/RelationshipDefinition.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::processors {

using core::logging::Logger;

class UnfocusArchiveEntry : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;
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
    io::IoResult operator()(const std::shared_ptr<io::OutputStream>& stream) const;
   private:
    //! Logger
    std::shared_ptr<Logger> logger_ = core::logging::LoggerFactory<UnfocusArchiveEntry>::getLogger();
    ArchiveMetadata *_archiveMetadata;
    static int ok_cb(struct archive *, void* /*d*/) { return ARCHIVE_OK; }
    static la_ssize_t write_cb(struct archive *, void *d, const void *buffer, size_t length);
  };
};

}  // namespace org::apache::nifi::minifi::processors
