/**
 * @file ProcessSession.cpp
 * ProcessSession class implementation
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
#include "core/ProcessSession.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/ProcessSessionReadCallback.h"
#include "utils/gsl.h"

/* This implementation is only for native Windows systems.  */
#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
#define _WINSOCKAPI_
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")
#include <direct.h>

int getpagesize(void) {
  return 4096;
  // SYSTEM_INFO system_info;
  // GetSystemInfo(&system_info);
  // return system_info.dwPageSize;
}
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::shared_ptr<utils::IdGenerator> ProcessSession::id_generator_ = utils::IdGenerator::getIdGenerator();

ProcessSession::~ProcessSession() {
  removeReferences();
}

void ProcessSession::add(const std::shared_ptr<core::FlowFile> &record) {
  utils::Identifier uuid = record->getUUID();
  if (_updatedFlowFiles.find(uuid) != _updatedFlowFiles.end()) {
    throw Exception(ExceptionType::PROCESSOR_EXCEPTION, "Mustn't add file that was provided by this session");
  }
  _addedFlowFiles[uuid] = record;
  record->setDeleted(false);
}

std::shared_ptr<core::FlowFile> ProcessSession::create(const std::shared_ptr<core::FlowFile> &parent) {
  auto record = std::make_shared<FlowFileRecord>();
  auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
  if (flow_version != nullptr) {
    record->setAttribute(SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
  }

  if (parent) {
    // Copy attributes
    for (const auto& attribute : parent->getAttributes()) {
      if (attribute.first == SpecialFlowAttribute::ALTERNATE_IDENTIFIER || attribute.first == SpecialFlowAttribute::DISCARD_REASON || attribute.first == SpecialFlowAttribute::UUID) {
        // Do not copy special attributes from parent
        continue;
      }
      record->setAttribute(attribute.first, attribute.second);
    }
    record->setLineageStartDate(parent->getlineageStartDate());
    record->setLineageIdentifiers(parent->getlineageIdentifiers());
    parent->getlineageIdentifiers().push_back(parent->getUUID());
  }

  utils::Identifier uuid = record->getUUID();
  _addedFlowFiles[uuid] = record;
  logger_->log_debug("Create FlowFile with UUID %s", record->getUUIDStr());
  std::stringstream details;
  details << process_context_->getProcessorNode()->getName() << " creates flow record " << record->getUUIDStr();
  provenance_report_->create(record, details.str());

  return record;
}

std::shared_ptr<core::FlowFile> ProcessSession::clone(const std::shared_ptr<core::FlowFile> &parent) {
  std::shared_ptr<core::FlowFile> record = this->create(parent);
  if (record) {
    logger_->log_debug("Cloned parent flow files %s to %s", parent->getUUIDStr(), record->getUUIDStr());
    // Copy Resource Claim
    std::shared_ptr<ResourceClaim> parent_claim = parent->getResourceClaim();
    record->setResourceClaim(parent_claim);
    if (parent_claim) {
      record->setOffset(parent->getOffset());
      record->setSize(parent->getSize());
    }
    provenance_report_->clone(parent, record);
  }
  return record;
}

std::shared_ptr<core::FlowFile> ProcessSession::cloneDuringTransfer(const std::shared_ptr<core::FlowFile> &parent) {
  auto record = std::make_shared<FlowFileRecord>();

  auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
  if (flow_version != nullptr) {
    record->setAttribute(SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
  }
  this->_clonedFlowFiles.push_back(record);
  logger_->log_debug("Clone FlowFile with UUID %s during transfer", record->getUUIDStr());
  // Copy attributes
  for (const auto& attribute : parent->getAttributes()) {
    if (attribute.first == SpecialFlowAttribute::ALTERNATE_IDENTIFIER
        || attribute.first == SpecialFlowAttribute::DISCARD_REASON
        || attribute.first == SpecialFlowAttribute::UUID) {
      // Do not copy special attributes from parent
      continue;
    }
    record->setAttribute(attribute.first, attribute.second);
  }
  record->setLineageStartDate(parent->getlineageStartDate());
  record->setLineageIdentifiers(parent->getlineageIdentifiers());
  record->getlineageIdentifiers().push_back(parent->getUUID());

  // Copy Resource Claim
  std::shared_ptr<ResourceClaim> parent_claim = parent->getResourceClaim();
  record->setResourceClaim(parent_claim);
  if (parent_claim != nullptr) {
    record->setOffset(parent->getOffset());
    record->setSize(parent->getSize());
  }
  provenance_report_->clone(parent, record);

  return record;
}

std::shared_ptr<core::FlowFile> ProcessSession::clone(const std::shared_ptr<core::FlowFile> &parent, int64_t offset, int64_t size) {
  if ((uint64_t) (offset + size) > parent->getSize()) {
    // Set offset and size
    logger_->log_error("clone offset %" PRId64 " and size %" PRId64 " exceed parent size %" PRIu64, offset, size, parent->getSize());
    return nullptr;
  }
  std::shared_ptr<core::FlowFile> record = this->create(parent);
  if (record) {
    logger_->log_debug("Cloned parent flow files %s to %s, with %u:%u", parent->getUUIDStr(), record->getUUIDStr(), offset, size);
    if (parent->getResourceClaim()) {
      record->setOffset(parent->getOffset() + offset);
      record->setSize(size);
      // Copy Resource Claim
      record->setResourceClaim(parent->getResourceClaim());
    }
    provenance_report_->clone(parent, record);
  }
  return record;
}

void ProcessSession::remove(const std::shared_ptr<core::FlowFile> &flow) {
  flow->setDeleted(true);
  _deletedFlowFiles.push_back(flow);
  std::string reason = process_context_->getProcessorNode()->getName() + " drop flow record " + flow->getUUIDStr();
  provenance_report_->drop(flow, reason);
}

void ProcessSession::putAttribute(const std::shared_ptr<core::FlowFile> &flow, std::string key, std::string value) {
  flow->setAttribute(key, value);
  std::stringstream details;
  details << process_context_->getProcessorNode()->getName() << " modify flow record " << flow->getUUIDStr() << " attribute " << key << ":" << value;
  provenance_report_->modifyAttributes(flow, details.str());
}

void ProcessSession::removeAttribute(const std::shared_ptr<core::FlowFile> &flow, std::string key) {
  flow->removeAttribute(key);
  std::stringstream details;
  details << process_context_->getProcessorNode()->getName() << " remove flow record " << flow->getUUIDStr() << " attribute " + key;
  provenance_report_->modifyAttributes(flow, details.str());
}

void ProcessSession::penalize(const std::shared_ptr<core::FlowFile> &flow) {
  const std::chrono::milliseconds penalization_period = process_context_->getProcessorNode()->getPenalizationPeriod();
  logging::LOG_INFO(logger_) << "Penalizing " << flow->getUUIDStr() << " for " << penalization_period.count() << "ms at " << process_context_->getProcessorNode()->getName();
  flow->penalize(penalization_period);
}

void ProcessSession::transfer(const std::shared_ptr<core::FlowFile> &flow, Relationship relationship) {
  logging::LOG_INFO(logger_) << "Transferring " << flow->getUUIDStr() << " from " << process_context_->getProcessorNode()->getName() << " to relationship " << relationship.getName();
  utils::Identifier uuid = flow->getUUID();
  _transferRelationship[uuid] = relationship;
  flow->setDeleted(false);
}

void ProcessSession::write(const std::shared_ptr<core::FlowFile> &flow, OutputStreamCallback *callback) {
  std::shared_ptr<ResourceClaim> claim = content_session_->create();

  try {
    uint64_t startTime = utils::timeutils::getTimeMillis();
    std::shared_ptr<io::BaseStream> stream = content_session_->write(claim);
    // Call the callback to write the content
    if (nullptr == stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for write");
    }
    if (callback->process(stream) < 0) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
    }

    flow->setSize(stream->size());
    flow->setOffset(0);
    flow->setResourceClaim(claim);

    stream->close();
    std::string details = process_context_->getProcessorNode()->getName() + " modify flow record content " + flow->getUUIDStr();
    uint64_t endTime = utils::timeutils::getTimeMillis();
    provenance_report_->modifyContent(flow, details, endTime - startTime);
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session write");
    throw;
  }
}

void ProcessSession::writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, gsl::span<const char> buffer) {
  struct BufferOutputStreamCallback : OutputStreamCallback {
    explicit BufferOutputStreamCallback(gsl::span<const char> buffer) :buffer{buffer} {}
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) final {
      return stream->write(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
    }
    gsl::span<const char> buffer;
  };
  BufferOutputStreamCallback cb{ buffer };
  write(flow_file, &cb);
}

void ProcessSession::append(const std::shared_ptr<core::FlowFile> &flow, OutputStreamCallback *callback) {
  std::shared_ptr<ResourceClaim> claim = flow->getResourceClaim();
  if (!claim) {
    // No existed claim for append, we need to create new claim
    return write(flow, callback);
  }

  try {
    uint64_t startTime = utils::timeutils::getTimeMillis();
    std::shared_ptr<io::BaseStream> stream = content_session_->write(claim, ContentSession::WriteMode::APPEND);
    if (nullptr == stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for append");
    }
    // Call the callback to write the content

    size_t oldPos = stream->size();
    // this prevents an issue if we write, above, with zero length.
    if (oldPos > 0)
      stream->seek(oldPos + 1);
    if (callback->process(stream) < 0) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
    }
    flow->setSize(stream->size());

    std::stringstream details;
    details << process_context_->getProcessorNode()->getName() << " modify flow record content " << flow->getUUIDStr();
    uint64_t endTime = utils::timeutils::getTimeMillis();
    provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session append");
    throw;
  }
}

int ProcessSession::read(const std::shared_ptr<core::FlowFile> &flow, InputStreamCallback *callback) {
  try {
    std::shared_ptr<ResourceClaim> claim = nullptr;

    if (flow->getResourceClaim() == nullptr) {
      // No existed claim for read, we throw exception
      logger_->log_debug("For %s, no resource claim but size is %d", flow->getUUIDStr(), flow->getSize());
      if (flow->getSize() == 0) {
        return 0;
      }
      throw Exception(FILE_OPERATION_EXCEPTION, "No Content Claim existed for read");
    }

    claim = flow->getResourceClaim();

    std::shared_ptr<io::BaseStream> stream = content_session_->read(claim);

    if (nullptr == stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for read");
    }

    stream->seek(flow->getOffset());

    auto ret = callback->process(stream);
    if (ret < 0) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
    }
    return gsl::narrow<int>(ret);
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session read");
    throw;
  }
}

void ProcessSession::importFrom(io::InputStream&& stream, const std::shared_ptr<core::FlowFile> &flow) {
  importFrom(stream, flow);
}
/**
 * Imports a file from the data stream
 * @param stream incoming data stream that contains the data to store into a file
 * @param flow flow file
 *
 */
void ProcessSession::importFrom(io::InputStream &stream, const std::shared_ptr<core::FlowFile> &flow) {
  const std::shared_ptr<ResourceClaim> claim = content_session_->create();
  const auto max_read = gsl::narrow_cast<size_t>(getpagesize());
  std::vector<uint8_t> charBuffer(max_read);

  try {
    auto startTime = utils::timeutils::getTimeMillis();
    std::shared_ptr<io::BaseStream> content_stream = content_session_->write(claim);

    if (nullptr == content_stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Could not obtain claim for " + claim->getContentFullPath());
    }
    size_t position = 0;
    const auto max_size = stream.size();
    while (position < max_size) {
      const auto read_size = std::min(max_read, max_size - position);
      stream.read(charBuffer, read_size);

      content_stream->write(charBuffer.data(), read_size);
      position += read_size;
    }
    // Open the source file and stream to the flow file

    flow->setSize(content_stream->size());
    flow->setOffset(0);
    flow->setResourceClaim(claim);

    logger_->log_debug("Import offset %" PRIu64 " length %" PRIu64 " into content %s for FlowFile UUID %s",
        flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath(), flow->getUUIDStr());

    content_stream->close();
    std::stringstream details;
    details << process_context_->getProcessorNode()->getName() << " modify flow record content " << flow->getUUIDStr();
    auto endTime = utils::timeutils::getTimeMillis();
    provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session write");
    throw;
  }
}

void ProcessSession::import(std::string source, const std::shared_ptr<FlowFile> &flow, bool keepSource, uint64_t offset) {
  std::shared_ptr<ResourceClaim> claim = content_session_->create();
  size_t size = getpagesize();
  std::vector<uint8_t> charBuffer(size);

  try {
    auto startTime = utils::timeutils::getTimeMillis();
    std::ifstream input;
    input.open(source.c_str(), std::fstream::in | std::fstream::binary);
    std::shared_ptr<io::BaseStream> stream = content_session_->write(claim);

    if (nullptr == stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open new flowfile content for write");
    }
    if (input.is_open() && input.good()) {
      bool invalidWrite = false;
      // Open the source file and stream to the flow file
      if (offset != 0) {
        input.seekg(offset);
        if (!input.good()) {
          logger_->log_error("Seeking to %d failed for file %s (does file/filesystem support seeking?)", offset, source);
          invalidWrite = true;
        }
      }
      while (input.good()) {
        input.read(reinterpret_cast<char*>(charBuffer.data()), size);
        if (input) {
          if (stream->write(charBuffer.data(), gsl::narrow<int>(size)) < 0) {
            invalidWrite = true;
            break;
          }
        } else {
          if (stream->write(reinterpret_cast<uint8_t*>(charBuffer.data()), gsl::narrow<int>(input.gcount())) < 0) {
            invalidWrite = true;
            break;
          }
        }
      }

      if (!invalidWrite) {
        flow->setSize(stream->size());
        flow->setOffset(0);
        flow->setResourceClaim(claim);

        logger_->log_debug("Import offset %" PRIu64 " length %" PRIu64 " into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath(),
                           flow->getUUIDStr());

        stream->close();
        input.close();
        if (!keepSource)
          std::remove(source.c_str());
        std::stringstream details;
        details << process_context_->getProcessorNode()->getName() << " modify flow record content " << flow->getUUIDStr();
        auto endTime = utils::timeutils::getTimeMillis();
        provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
      } else {
        stream->close();
        input.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session write");
    throw;
  }
}

void ProcessSession::import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, uint64_t offset, char inputDelimiter) {
  std::shared_ptr<ResourceClaim> claim;
  std::shared_ptr<io::BaseStream> stream;
  std::shared_ptr<core::FlowFile> flowFile;

  std::vector<uint8_t> buffer(getpagesize());
  try {
    std::ifstream input{source, std::ios::in | std::ios::binary};
    logger_->log_debug("Opening %s", source);
    if (!input.is_open() || !input.good()) {
      throw Exception(FILE_OPERATION_EXCEPTION, utils::StringUtils::join_pack("File Import Error: failed to open file \'", source, "\'"));
    }
    if (offset != 0U) {
      input.seekg(offset, std::ifstream::beg);
      if (!input.good()) {
        logger_->log_error("Seeking to %lu failed for file %s (does file/filesystem support seeking?)", offset, source);
        throw Exception(FILE_OPERATION_EXCEPTION, utils::StringUtils::join_pack("File Import Error: Couldn't seek to offset ", std::to_string(offset)));
      }
    }
    uint64_t startTime = 0U;
    while (input.good()) {
      input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
      std::streamsize read = input.gcount();
      if (read < 0) {
        throw Exception(FILE_OPERATION_EXCEPTION, "std::ifstream::gcount returned negative value");
      }
      if (read == 0) {
        logger_->log_trace("Finished reading input %s", source);
        break;
      } else {
        logging::LOG_TRACE(logger_) << "Read input of " << read;
      }
      uint8_t* begin = buffer.data();
      uint8_t* end = begin + read;
      while (true) {
        startTime = utils::timeutils::getTimeMillis();
        uint8_t* delimiterPos = std::find(begin, end, static_cast<uint8_t>(inputDelimiter));
        const auto len = gsl::narrow<int>(delimiterPos - begin);

        logging::LOG_TRACE(logger_) << "Read input of " << read << " length is " << len << " is at end?" << (delimiterPos == end);
        /*
         * We do not want to process the rest of the buffer after the last delimiter if
         *  - we have reached EOF in the file (we would discard it anyway)
         *  - there is nothing to process (the last character in the buffer is a delimiter)
         */
        if (delimiterPos == end && (input.eof() || len == 0)) {
          break;
        }

        /* Create claim and stream if needed and append data */
        if (claim == nullptr) {
          startTime = utils::timeutils::getTimeMillis();
          claim = content_session_->create();
        }
        if (stream == nullptr) {
          stream = content_session_->write(claim);
        }
        if (stream == nullptr) {
          logger_->log_error("Stream is null");
          throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for import");
        }
        if (stream->write(begin, len) != len) {
          logger_->log_error("Error while writing");
          stream->close();
          throw Exception(FILE_OPERATION_EXCEPTION, "File Export Error creating Flowfile");
        }

        /* Create a FlowFile if we reached a delimiter */
        if (delimiterPos == end) {
          break;
        }
        flowFile = create();
        flowFile->setSize(stream->size());
        flowFile->setOffset(0);
        flowFile->setResourceClaim(claim);
        logging::LOG_DEBUG(logger_) << "Import offset " << flowFile->getOffset() << " length " << flowFile->getSize() << " content " << flowFile->getResourceClaim()->getContentFullPath()
                                    << ", FlowFile UUID " << flowFile->getUUIDStr();
        stream->close();
        std::string details = process_context_->getProcessorNode()->getName() + " modify flow record content " + flowFile->getUUIDStr();
        uint64_t endTime = utils::timeutils::getTimeMillis();
        provenance_report_->modifyContent(flowFile, details, endTime - startTime);
        flows.push_back(flowFile);

        /* Reset these to start processing the next FlowFile with a clean slate */
        flowFile.reset();
        stream.reset();
        claim.reset();

        /* Skip delimiter */
        begin = delimiterPos + 1;
      }
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session write");
    throw;
  }
}

void ProcessSession::import(std::string source, std::vector<std::shared_ptr<FlowFile>> &flows, bool keepSource, uint64_t offset, char inputDelimiter) {
// this function calls a deprecated function, but it is itself deprecated, so suppress warnings
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(WIN32)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
  import(source, flows, offset, inputDelimiter);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(WIN32)
#pragma warning(pop)
#endif
  logger_->log_trace("Closed input %s, keeping source ? %i", source, keepSource);
  if (!keepSource) {
    std::remove(source.c_str());
  }
}

bool ProcessSession::exportContent(const std::string &destination, const std::string &tmpFile, const std::shared_ptr<core::FlowFile> &flow, bool /*keepContent*/) {
  logger_->log_debug("Exporting content of %s to %s", flow->getUUIDStr(), destination);

  ProcessSessionReadCallback cb(tmpFile, destination, logger_);
  read(flow, &cb);

  logger_->log_info("Committing %s", destination);
  bool commit_ok = cb.commit();

  if (commit_ok) {
    logger_->log_info("Commit OK.");
  } else {
    logger_->log_error("Commit of %s to %s failed!", flow->getUUIDStr(), destination);
  }
  return commit_ok;
}

bool ProcessSession::exportContent(const std::string &destination, const std::shared_ptr<core::FlowFile> &flow, bool keepContent) {
  utils::Identifier tmpFileUuid = id_generator_->generate();
  std::stringstream tmpFileSs;
  tmpFileSs << destination << "." << tmpFileUuid.to_string();
  std::string tmpFileName = tmpFileSs.str();

  return exportContent(destination, tmpFileName, flow, keepContent);
}

void ProcessSession::stash(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) {
  logger_->log_debug("Stashing content from %s to key %s", flow->getUUIDStr(), key);

  auto claim = flow->getResourceClaim();
  if (!claim) {
    logger_->log_warn("Attempted to stash content of record %s when "
                      "there is no resource claim",
                      flow->getUUIDStr());
    return;
  }

  // Stash the claim
  flow->setStashClaim(key, claim);

  // Clear current claim
  flow->clearResourceClaim();
}

void ProcessSession::restore(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) {
  logger_->log_info("Restoring content to %s from key %s", flow->getUUIDStr(), key);

  // Restore the claim
  if (!flow->hasStashClaim(key)) {
    logger_->log_warn("Requested restore to record %s from unknown key %s", flow->getUUIDStr(), key);
    return;
  }

  // Disown current claim if existing
  if (flow->getResourceClaim()) {
    logger_->log_warn("Restoring stashed content of record %s from key %s when there is "
                      "existing content; existing content will be overwritten",
                      flow->getUUIDStr(), key);
  }

  // Restore the claim
  auto stashClaim = flow->getStashClaim(key);
  flow->setResourceClaim(stashClaim);
  flow->clearStashClaim(key);
}

ProcessSession::RouteResult ProcessSession::routeFlowFile(const std::shared_ptr<FlowFile> &record) {
  if (record->isDeleted()) {
    return RouteResult::Ok_Deleted;
  }
  utils::Identifier uuid = record->getUUID();
  auto itRelationship = _transferRelationship.find(uuid);
  if (itRelationship == _transferRelationship.end()) {
    return RouteResult::Error_NoRelationship;
  }
  Relationship relationship = itRelationship->second;
  // Find the relationship, we need to find the connections for that relationship
  std::set<std::shared_ptr<Connectable>> connections = process_context_->getProcessorNode()->getOutGoingConnections(relationship.getName());
  if (connections.empty()) {
    // No connection
    if (!process_context_->getProcessorNode()->isAutoTerminated(relationship)) {
      // Not autoterminate, we should have the connect
      std::string message = "Connect empty for non auto terminated relationship " + relationship.getName();
      throw Exception(PROCESS_SESSION_EXCEPTION, message);
    } else {
      // Autoterminated
      remove(record);
    }
  } else {
    // We connections, clone the flow and assign the connection accordingly
    for (auto itConnection = connections.begin(); itConnection != connections.end(); ++itConnection) {
      std::shared_ptr<Connectable> connection = *itConnection;
      if (itConnection == connections.begin()) {
        // First connection which the flow need be routed to
        record->setConnection(connection);
      } else {
        // Clone the flow file and route to the connection
        std::shared_ptr<core::FlowFile> cloneRecord = this->cloneDuringTransfer(record);
        if (cloneRecord)
          cloneRecord->setConnection(connection);
        else
          throw Exception(PROCESS_SESSION_EXCEPTION, "Can not clone the flow for transfer " + record->getUUIDStr());
      }
    }
  }
  return RouteResult::Ok_Routed;
}

void ProcessSession::commit() {
  try {
    // First we clone the flow record based on the transferred relationship for updated flow record
    for (auto && it : _updatedFlowFiles) {
      auto record = it.second.modified;
      if (routeFlowFile(record) == RouteResult::Error_NoRelationship) {
        // Can not find relationship for the flow
        throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the updated flow " + record->getUUIDStr());
      }
    }

    // Do the same thing for added flow file
    for (const auto& it : _addedFlowFiles) {
      auto record = it.second;
      if (routeFlowFile(record) == RouteResult::Error_NoRelationship) {
        // Can not find relationship for the flow
        throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the added flow " + record->getUUIDStr());
      }
    }

    std::map<std::shared_ptr<Connectable>, std::vector<std::shared_ptr<FlowFile>>> connectionQueues;

    std::shared_ptr<Connectable> connection = nullptr;
    // Complete process the added and update flow files for the session, send the flow file to its queue
    for (const auto &it : _updatedFlowFiles) {
      auto record = it.second.modified;
      logger_->log_trace("See %s in %s", record->getUUIDStr(), "_updatedFlowFiles");
      if (record->isDeleted()) {
        continue;
      }

      connection = record->getConnection();
      if ((connection) != nullptr) {
        connectionQueues[connection].push_back(record);
      }
    }
    for (const auto &it : _addedFlowFiles) {
      auto record = it.second;
      logger_->log_trace("See %s in %s", record->getUUIDStr(), "_addedFlowFiles");
      if (record->isDeleted()) {
        continue;
      }
      connection = record->getConnection();
      if ((connection) != nullptr) {
        connectionQueues[connection].push_back(record);
      }
    }
    // Process the clone flow files
    for (const auto &record : _clonedFlowFiles) {
      logger_->log_trace("See %s in %s", record->getUUIDStr(), "_clonedFlowFiles");
      if (record->isDeleted()) {
        continue;
      }
      connection = record->getConnection();
      if ((connection) != nullptr) {
        connectionQueues[connection].push_back(record);
      }
    }

    for (const auto& record : _deletedFlowFiles) {
        if (!record->isDeleted()) {
          continue;
        }
        if (record->isStored() && process_context_->getFlowFileRepository()->Delete(record->getUUIDStr())) {
          // mark for deletion in the flowFileRepository
          record->setStoredToRepository(false);
        }
    }

    ensureNonNullResourceClaim(connectionQueues);

    content_session_->commit();

    persistFlowFilesBeforeTransfer(connectionQueues, _updatedFlowFiles);

    for (auto& cq : connectionQueues) {
      auto connection = std::dynamic_pointer_cast<Connection>(cq.first);
      if (connection) {
        connection->multiPut(cq.second);
      } else {
        for (auto& file : cq.second) {
          cq.first->put(file);
        }
      }
    }

    // All done
    _updatedFlowFiles.clear();
    _addedFlowFiles.clear();
    _clonedFlowFiles.clear();
    _deletedFlowFiles.clear();

    _transferRelationship.clear();
    // persistent the provenance report
    this->provenance_report_->commit();
    logger_->log_trace("ProcessSession committed for %s", process_context_->getProcessorNode()->getName());
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session commit");
    throw;
  }
}

void ProcessSession::rollback() {
  // new FlowFiles are only persisted during commit
  // no need to delete them here
  std::map<std::shared_ptr<Connectable>, std::vector<std::shared_ptr<FlowFile>>> connectionQueues;

  try {
    // Requeue the snapshot of the flowfile back
    for (const auto &it : _updatedFlowFiles) {
      auto flowFile = it.second.modified;
      // restore flowFile to original state
      *flowFile = *it.second.snapshot;
      penalize(flowFile);
      logger_->log_debug("ProcessSession rollback for %s, record %s, to connection %s",
          process_context_->getProcessorNode()->getName(),
          flowFile->getUUIDStr(),
          flowFile->getConnection()->getName());
      connectionQueues[flowFile->getConnection()].push_back(flowFile);
    }

    for (const auto& record : _deletedFlowFiles) {
      record->setDeleted(false);
    }

    // put everything back where it came from
    for (auto& cq : connectionQueues) {
      auto connection = std::dynamic_pointer_cast<Connection>(cq.first);
      if (connection) {
        connection->multiPut(cq.second);
      } else {
        for (auto& flow : cq.second) {
          cq.first->put(flow);
        }
      }
    }

    content_session_->rollback();

    _clonedFlowFiles.clear();
    _addedFlowFiles.clear();
    _updatedFlowFiles.clear();
    _deletedFlowFiles.clear();
    logger_->log_warn("ProcessSession rollback for %s executed", process_context_->getProcessorNode()->getName());
  } catch (std::exception &exception) {
    logger_->log_warn("Caught Exception during process session rollback: %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_warn("Caught Exception during process session rollback");
    throw;
  }
}

void ProcessSession::persistFlowFilesBeforeTransfer(
    std::map<std::shared_ptr<Connectable>, std::vector<std::shared_ptr<core::FlowFile> > >& transactionMap,
    const std::map<utils::Identifier, FlowFileUpdate>& modifiedFlowFiles) {

  std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>> flowData;

  auto flowFileRepo = process_context_->getFlowFileRepository();
  auto contentRepo = process_context_->getContentRepository();

  for (auto& transaction : transactionMap) {
    const std::shared_ptr<Connectable>& target = transaction.first;
    std::shared_ptr<Connection> connection = std::dynamic_pointer_cast<Connection>(target);
    const bool shouldDropEmptyFiles = connection ? connection->getDropEmptyFlowFiles() : false;
    auto& flows = transaction.second;
    for (auto &ff : flows) {
      if (shouldDropEmptyFiles && ff->getSize() == 0) {
        // the receiver will drop this FF
        continue;
      }

      std::unique_ptr<io::BufferStream> stream(new io::BufferStream());
      std::static_pointer_cast<FlowFileRecord>(ff)->Serialize(*stream);

      flowData.emplace_back(ff->getUUIDStr(), std::move(stream));
    }
  }

  if (!flowFileRepo->MultiPut(flowData)) {
    logger_->log_error("Failed execute multiput on FF repo!");
    throw Exception(PROCESS_SESSION_EXCEPTION, "Failed to put flowfiles to repository");
  }

  for (auto& transaction : transactionMap) {
    const std::shared_ptr<Connectable>& target = transaction.first;
    std::shared_ptr<Connection> connection = std::dynamic_pointer_cast<Connection>(target);
    const bool shouldDropEmptyFiles = connection ? connection->getDropEmptyFlowFiles() : false;
    auto& flows = transaction.second;
    for (auto &ff : flows) {
      utils::Identifier uuid = ff->getUUID();
      auto snapshotIt = modifiedFlowFiles.find(uuid);
      auto original = snapshotIt != modifiedFlowFiles.end() ? snapshotIt->second.snapshot : nullptr;
      if (shouldDropEmptyFiles && ff->getSize() == 0) {
        // the receiver promised to drop this FF, no need for it anymore
        if (ff->isStored() && flowFileRepo->Delete(ff->getUUIDStr())) {
          // original must be non-null since this flowFile is already stored in the repos ->
          // must have come from a session->get()
          assert(original);
          ff->setStoredToRepository(false);
        }
        continue;
      }
      auto claim = ff->getResourceClaim();
      // increment on behalf of the persisted instance
      if (claim) claim->increaseFlowFileRecordOwnedCount();
      auto originalClaim = original ? original->getResourceClaim() : nullptr;
      // decrement on behalf of the overridden instance if any
      if (originalClaim) originalClaim->decreaseFlowFileRecordOwnedCount();

      ff->setStoredToRepository(true);
    }
  }
}

void ProcessSession::ensureNonNullResourceClaim(
    const std::map<std::shared_ptr<Connectable>, std::vector<std::shared_ptr<core::FlowFile>>> &transactionMap) {
  for (auto& transaction : transactionMap) {
    for (auto& flowFile : transaction.second) {
      auto claim = flowFile->getResourceClaim();
      if (!claim) {
        logger_->log_debug("Processor %s (%s) did not create a ResourceClaim, creating an empty one",
                           process_context_->getProcessorNode()->getUUIDStr(),
                           process_context_->getProcessorNode()->getName());
        OutputStreamPipe emptyStreamCallback(std::make_shared<io::BufferStream>());
        write(flowFile, &emptyStreamCallback);
      }
    }
  }
}

std::shared_ptr<core::FlowFile> ProcessSession::get() {
  std::shared_ptr<Connectable> first = process_context_->getProcessorNode()->pickIncomingConnection();

  if (first == nullptr) {
    logger_->log_trace("Get is null for %s", process_context_->getProcessorNode()->getName());
    return nullptr;
  }

  std::shared_ptr<Connection> current = std::static_pointer_cast<Connection>(first);

  do {
    std::set<std::shared_ptr<core::FlowFile> > expired;
    std::shared_ptr<core::FlowFile> ret = current->poll(expired);
    if (!expired.empty()) {
      // Remove expired flow record
      for (const auto& record : expired) {
        std::stringstream details;
        details << process_context_->getProcessorNode()->getName() << " expire flow record " << record->getUUIDStr();
        provenance_report_->expire(record, details.str());
        // there is no rolling back expired FlowFiles
        if (record->isStored() && process_context_->getFlowFileRepository()->Delete(record->getUUIDStr())) {
          record->setStoredToRepository(false);
        }
      }
    }
    if (ret) {
      // add the flow record to the current process session update map
      ret->setDeleted(false);
      std::shared_ptr<FlowFile> snapshot = std::make_shared<FlowFileRecord>();
      *snapshot = *ret;
      logger_->log_debug("Create Snapshot FlowFile with UUID %s", snapshot->getUUIDStr());
      utils::Identifier uuid = ret->getUUID();
      _updatedFlowFiles[uuid] = {ret, snapshot};
      auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
      if (flow_version != nullptr) {
        ret->setAttribute(SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
      }
      return ret;
    }
    current = std::static_pointer_cast<Connection>(process_context_->getProcessorNode()->pickIncomingConnection());
  } while (current != nullptr && current != first);

  return nullptr;
}

void ProcessSession::flushContent() {
  content_session_->commit();
}

bool ProcessSession::outgoingConnectionsFull(const std::string& relationship) {
  std::set<std::shared_ptr<Connectable>> connections = process_context_->getProcessorNode()->getOutGoingConnections(relationship);
  Connection * connection = nullptr;
  for (const auto& conn : connections) {
    connection = dynamic_cast<Connection*>(conn.get());
    if (connection && connection->isFull()) {
      return true;
    }
  }
  return false;
}

bool ProcessSession::existsFlowFileInRelationship(const Relationship &relationship) {
  return std::any_of(_transferRelationship.begin(), _transferRelationship.end(),
      [&relationship](const std::map<utils::Identifier, Relationship>::value_type &key_value_pair) {
        return relationship == key_value_pair.second;
  });
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
