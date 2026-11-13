// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "PutSmb.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/OsUtils.h"
#include "utils/file/FileWriterCallback.h"
#include "core/Resource.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::extensions::smb {

void PutSmb::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutSmb::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  smb_connection_controller_service_ = utils::parseControllerService<SmbConnectionControllerService>(context, PutSmb::ConnectionControllerService, getUUID());
  create_missing_dirs_ = utils::parseBoolProperty(context, PutSmb::CreateMissingDirectories);
  conflict_resolution_strategy_ = utils::parseEnumProperty<FileExistsResolutionStrategy>(context, ConflictResolution);
}

std::filesystem::path PutSmb::getFilePath(core::ProcessContext& context, const core::FlowFile& flow_file) {
  auto filename = flow_file.getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(flow_file.getUUIDStr());
  return smb_connection_controller_service_->getPath() / context.getProperty(Directory, &flow_file).value_or("") / filename;
}

void PutSmb::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(smb_connection_controller_service_);

  if (auto connection_error = smb_connection_controller_service_->validateConnection()) {
    logger_->log_error("Couldn't establish connection to the specified network location due to {}", connection_error.message());
    context.yield();
    return;
  }

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto full_file_path = getFilePath(context, *flow_file);

  if (utils::file::exists(full_file_path)) {
    logger_->log_info("Destination file {} exists; applying Conflict Resolution Strategy: {}", full_file_path, magic_enum::enum_name(conflict_resolution_strategy_));
    if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::fail) {
      session.transfer(flow_file, Failure);
      return;
    } else if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::ignore) {
      session.transfer(flow_file, Success);
      return;
    }
  }

  if (!utils::file::exists(full_file_path.parent_path()) && create_missing_dirs_) {
    logger_->log_debug("Destination directory does not exist; will attempt to create: {}", full_file_path.parent_path());
    utils::file::create_dir(full_file_path.parent_path(), true);
  }

  bool success = false;

  utils::FileWriterCallback file_writer_callback(full_file_path);
  auto read_result = session.read(flow_file, std::ref(file_writer_callback));
  if (io::isError(read_result)) {
    logger_->log_error("Failed to write to {}", full_file_path);
    success = false;
  } else {
    success = file_writer_callback.commit();
  }

  session.transfer(flow_file, success ? Success : Failure);
}

REGISTER_RESOURCE(PutSmb, Processor);

}  // namespace org::apache::nifi::minifi::extensions::smb
