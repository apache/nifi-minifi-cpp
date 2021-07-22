/**
 * @file ManipulateArchive.cpp
 * ManipulateArchive class implementation
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
#include <iostream>
#include <memory>
#include <string>
#include <set>
#include <list>
#include <algorithm>

#include "archive.h"
#include "archive_entry.h"

#include "ManipulateArchive.h"
#include "Exception.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"
#include "utils/file/FileManager.h"
#include "FocusArchiveEntry.h"
#include "UnfocusArchiveEntry.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ManipulateArchive::Operation("Operation", "Operation to perform on the archive (touch, remove, copy, move).", "");
core::Property ManipulateArchive::Target("Target", "An existing entry within the archive to perform the operation on.", "");
core::Property ManipulateArchive::Destination("Destination", "Destination for operations (touch, move or copy) which result in new entries.", "");
core::Property ManipulateArchive::Before("Before", "For operations which result in new entries, places the new entry before the entry specified by this property.", "");
core::Property ManipulateArchive::After("After", "For operations which result in new entries, places the new entry after the entry specified by this property.", "");
core::Relationship ManipulateArchive::Success("success", "FlowFiles will be transferred to the success relationship if the operation succeeds.");
core::Relationship ManipulateArchive::Failure("failure", "FlowFiles will be transferred to the failure relationship if the operation fails.");

char const* ManipulateArchive::OPERATION_REMOVE = "remove";
char const* ManipulateArchive::OPERATION_COPY =   "copy";
char const* ManipulateArchive::OPERATION_MOVE =   "move";
char const* ManipulateArchive::OPERATION_TOUCH =  "touch";

void ManipulateArchive::initialize() {
    //! Set the supported properties
    std::set<core::Property> properties;
    properties.insert(Operation);
    properties.insert(Target);
    properties.insert(Destination);
    properties.insert(Before);
    properties.insert(After);
    setSupportedProperties(properties);

    //! Set the supported relationships
    std::set<core::Relationship> relationships;
    relationships.insert(Success);
    relationships.insert(Failure);
    setSupportedRelationships(relationships);
}

void ManipulateArchive::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
    context->getProperty(Operation.getName(), operation_);
    bool invalid = false;
    std::transform(operation_.begin(), operation_.end(), operation_.begin(), ::tolower);

    bool op_create = operation_ == OPERATION_COPY ||
                     operation_ == OPERATION_MOVE ||
                     operation_ == OPERATION_TOUCH;

    // Operation must be one of copy, move, touch or remove
    if (!op_create && (operation_ != OPERATION_REMOVE)) {
        logger_->log_error("Invalid operation %s for ManipulateArchive.", operation_);
        invalid = true;
    }

    context->getProperty(Target.getName(), targetEntry_);
    context->getProperty(Destination.getName(), destination_);
    context->getProperty(Before.getName(), before_);
    context->getProperty(After.getName(), after_);

    // All operations which create new entries require a set destination
    if (op_create == destination_.empty()) {
        logger_->log_error("ManipulateArchive requires a destination for %s.", operation_);
        invalid = true;
    }

    // The only operation that doesn't require an existing target is touch
    if ((operation_ == OPERATION_TOUCH) != targetEntry_.empty()) {
        logger_->log_error("ManipulateArchive requires a target for %s.", operation_);
        invalid = true;
    }

    // Users may specify one or none of before or after, but never both.
    if (before_.size() && after_.size()) {
        logger_->log_error("ManipulateArchive: cannot specify both before and after.");
        invalid = true;
    }

    if (invalid) {
        throw Exception(GENERAL_EXCEPTION, "Invalid ManipulateArchive configuration");
    }
}

void ManipulateArchive::onTrigger(core::ProcessContext* /*context*/, core::ProcessSession *session) {
    std::shared_ptr<core::FlowFile> flowFile = session->get();

    if (!flowFile) {
        return;
    }

    ArchiveMetadata archiveMetadata;
    fileutils::FileManager file_man;

    FocusArchiveEntry::ReadCallback readCallback(this, &file_man, &archiveMetadata);
    session->read(flowFile, &readCallback);

    auto entries_end = archiveMetadata.entryMetadata.end();

    auto target_position = archiveMetadata.find(targetEntry_);

    if (target_position == entries_end && operation_ != OPERATION_TOUCH) {
        logger_->log_warn("ManipulateArchive could not find entry %s to %s!",
                          targetEntry_, operation_);
        session->transfer(flowFile, Failure);
        return;
    } else {
        logger_->log_info("ManipulateArchive found %s for %s.",
                          targetEntry_, operation_);
    }

    if (!destination_.empty()) {
        auto dest_position = archiveMetadata.find(destination_);
        if (dest_position != entries_end) {
            logger_->log_warn("ManipulateArchive cannot perform %s to existing destination_ %s!",
                              operation_, destination_);
            session->transfer(flowFile, Failure);
            return;
        }
    }

    auto position = entries_end;

    // Small speedup for when neither before or after are provided or needed
    if ((!before_.empty() || !after_.empty()) && operation_ != OPERATION_REMOVE) {
        std::string positionEntry = after_.empty() ? before_ : after_;
        position = archiveMetadata.find(positionEntry);

        if (position == entries_end)
            logger_->log_warn("ManipulateArchive could not find entry %s to "
                              "perform %s %s; appending to end of archive...",
                              positionEntry, operation_,
                              after_.empty() ? "before" : "after");

        else
            logger_->log_info("ManipulateArchive found entry %s to %s %s.",
                              positionEntry, operation_,
                              after_.empty() ? "before" : "after");

        if (!after_.empty() && position != entries_end)
            position++;
    }

    if (operation_ == OPERATION_REMOVE) {
        std::remove((*target_position).tmpFileName.c_str());
        target_position = archiveMetadata.eraseEntry(target_position);
    } else if (operation_ == OPERATION_COPY) {
        ArchiveEntryMetadata copy = *target_position;

        // Copy tmp file
        const auto origTmpFileName = copy.tmpFileName;
        const auto newTmpFileName = file_man.unique_file(false);
        copy.tmpFileName = newTmpFileName;
        std::ifstream src(origTmpFileName, std::ios::binary);
        std::ofstream dst(newTmpFileName, std::ios::binary);
        dst << src.rdbuf();
        copy.entryName = destination_;

        archiveMetadata.entryMetadata.insert(position, copy);
    } else if (operation_ == OPERATION_MOVE) {
        ArchiveEntryMetadata moveEntry = *target_position;
        target_position = archiveMetadata.eraseEntry(target_position);
        moveEntry.entryName = destination_;
        archiveMetadata.entryMetadata.insert(position, moveEntry);
    } else if (operation_ == OPERATION_TOUCH) {
        ArchiveEntryMetadata touchEntry;
        touchEntry.entryName = destination_;
        touchEntry.entryType = AE_IFREG;
        touchEntry.entrySize = 0;
        touchEntry.entryMTime = time(nullptr);
        touchEntry.entryMTimeNsec = 0;
        touchEntry.entryUID = 0;
        touchEntry.entryGID = 0;
        touchEntry.entryPerm = 0777;

        archiveMetadata.entryMetadata.insert(position, touchEntry);
    }

    UnfocusArchiveEntry::WriteCallback writeCallback(&archiveMetadata);
    session->write(flowFile, &writeCallback);

    session->transfer(flowFile, Success);
}

REGISTER_RESOURCE(ManipulateArchive, "Performs an operation which manipulates an archive without needing to split the archive into multiple FlowFiles.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
