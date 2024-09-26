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
#include "io/StreamSlice.h"
#include "io/StreamPipe.h"
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
#else
#include <unistd.h>
#endif

namespace org::apache::nifi::minifi::core {

std::shared_ptr<utils::IdGenerator> ProcessSessionImpl::id_generator_ = utils::IdGenerator::getIdGenerator();

ProcessSessionImpl::ProcessSessionImpl(std::shared_ptr<ProcessContext> processContext)
        : process_context_(std::move(processContext)),
          logger_(logging::LoggerFactory<ProcessSession>::getLogger()),
          stateManager_(process_context_->hasStateManager() ? process_context_->getStateManager() : nullptr) {
  logger_->log_trace("ProcessSession created for {}", process_context_->getProcessorNode()->getName());
  auto repo = process_context_->getProvenanceRepository();
  provenance_report_ = std::make_shared<provenance::ProvenanceReporterImpl>(repo, process_context_->getProcessorNode()->getName(), process_context_->getProcessorNode()->getName());
  content_session_ = process_context_->getContentRepository()->createSession();

  if (stateManager_ && !stateManager_->beginTransaction()) {
    throw Exception(PROCESS_SESSION_EXCEPTION, "State manager transaction could not be initiated.");
  }
}

ProcessSessionImpl::~ProcessSessionImpl() {
  if (stateManager_ && stateManager_->isTransactionInProgress()) {
    logger_->log_critical("Session has ended without decision on state (commit or rollback).");
    std::terminate();
  }
  removeReferences();
}

void ProcessSessionImpl::add(const std::shared_ptr<core::FlowFile> &record) {
  utils::Identifier uuid = record->getUUID();
  if (updated_flowfiles_.find(uuid) != updated_flowfiles_.end()) {
    throw Exception(ExceptionType::PROCESSOR_EXCEPTION, "Mustn't add file that was provided by this session");
  }
  added_flowfiles_[uuid].flow_file = record;
  record->setDeleted(false);
}

std::shared_ptr<core::FlowFile> ProcessSessionImpl::create(const core::FlowFile* const parent) {
  auto record = std::make_shared<FlowFileRecordImpl>();
  auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
  if (flow_version != nullptr) {
    record->setAttribute(SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
  }

  if (parent) {
    for (const auto& attribute : parent->getAttributes()) {
      if (attribute.first == SpecialFlowAttribute::ALTERNATE_IDENTIFIER || attribute.first == SpecialFlowAttribute::DISCARD_REASON || attribute.first == SpecialFlowAttribute::UUID) {
        // Do not copy special attributes from parent
        continue;
      }
      record->setAttribute(attribute.first, attribute.second);
    }
    record->setLineageStartDate(parent->getlineageStartDate());
    record->setLineageIdentifiers(parent->getlineageIdentifiers());
    record->getlineageIdentifiers().push_back(parent->getUUID());
  }

  utils::Identifier uuid = record->getUUID();
  added_flowfiles_[uuid].flow_file = record;
  logger_->log_debug("Create FlowFile with UUID {}", record->getUUIDStr());
  std::stringstream details;
  details << process_context_->getProcessorNode()->getName() << " creates flow record " << record->getUUIDStr();
  provenance_report_->create(*record, details.str());

  return record;
}

std::shared_ptr<core::FlowFile> ProcessSessionImpl::clone(const core::FlowFile& parent) {
  std::shared_ptr<core::FlowFile> record = this->create(&parent);
  if (record) {
    logger_->log_debug("Cloned parent flow files {} to {}", parent.getUUIDStr(), record->getUUIDStr());
    // Copy Resource Claim
    std::shared_ptr<ResourceClaim> parent_claim = parent.getResourceClaim();
    record->setResourceClaim(parent_claim);
    if (parent_claim) {
      record->setOffset(parent.getOffset());
      record->setSize(parent.getSize());
    }
    provenance_report_->clone(parent, *record);
  }
  return record;
}

std::shared_ptr<core::FlowFile> ProcessSessionImpl::cloneDuringTransfer(const core::FlowFile& parent) {
  auto record = std::make_shared<FlowFileRecordImpl>();

  auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
  if (flow_version != nullptr) {
    record->setAttribute(SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
  }
  this->cloned_flowfiles_.push_back(record);
  logger_->log_debug("Clone FlowFile with UUID {} during transfer", record->getUUIDStr());
  // Copy attributes
  for (const auto& attribute : parent.getAttributes()) {
    if (attribute.first == SpecialFlowAttribute::ALTERNATE_IDENTIFIER
        || attribute.first == SpecialFlowAttribute::DISCARD_REASON
        || attribute.first == SpecialFlowAttribute::UUID) {
      // Do not copy special attributes from parent
      continue;
    }
    record->setAttribute(attribute.first, attribute.second);
  }
  record->setLineageStartDate(parent.getlineageStartDate());
  record->setLineageIdentifiers(parent.getlineageIdentifiers());
  record->getlineageIdentifiers().push_back(parent.getUUID());

  // Copy Resource Claim
  std::shared_ptr<ResourceClaim> parent_claim = parent.getResourceClaim();
  record->setResourceClaim(parent_claim);
  if (parent_claim != nullptr) {
    record->setOffset(parent.getOffset());
    record->setSize(parent.getSize());
  }
  provenance_report_->clone(parent, *record);

  return record;
}

std::shared_ptr<core::FlowFile> ProcessSessionImpl::clone(const FlowFile& parent, int64_t offset, int64_t size) {
  if (gsl::narrow<uint64_t>(offset + size) > parent.getSize()) {
    // Set offset and size
    logger_->log_error("clone offset {} and size {} exceed parent size {}", offset, size, parent.getSize());
    return nullptr;
  }
  std::shared_ptr<core::FlowFile> record = this->create(&parent);
  if (record) {
    logger_->log_debug("Cloned parent flow files {} to {}, with {}:{}", parent.getUUIDStr(), record->getUUIDStr(), offset, size);
    if (parent.getResourceClaim()) {
      write(record, [&] (const std::shared_ptr<io::OutputStream>& output) -> int64_t {
        return read(parent, [&] (const std::shared_ptr<io::InputStream>& input) -> int64_t {
          io::StreamSlice slice(input, offset, size);
          return minifi::internal::pipe(slice, *output);
        });
      });
    }
    provenance_report_->clone(parent, *record);
  }
  return record;
}

void ProcessSessionImpl::remove(const std::shared_ptr<core::FlowFile> &flow) {
  logger_->log_debug("Removing flow file with UUID: {}", flow->getUUIDStr());
  flow->setDeleted(true);
  deleted_flowfiles_.push_back(flow);
  std::string reason = process_context_->getProcessorNode()->getName() + " drop flow record " + flow->getUUIDStr();
  provenance_report_->drop(*flow, reason);
}

void ProcessSessionImpl::putAttribute(core::FlowFile& flow_file, std::string_view key, const std::string& value) {
  flow_file.setAttribute(key, value);
  std::string details = fmt::format("{} modify flow record {} attribute {}:{}", process_context_->getProcessorNode()->getName(), flow_file.getUUIDStr(), key, value);
  provenance_report_->modifyAttributes(flow_file, details);
}

void ProcessSessionImpl::removeAttribute(core::FlowFile& flow_file, std::string_view key) {
  flow_file.removeAttribute(key);
  std::string details = fmt::format("{} remove flow record {} attribute {}", process_context_->getProcessorNode()->getName(), flow_file.getUUIDStr(), key);
  provenance_report_->modifyAttributes(flow_file, details);
}

void ProcessSessionImpl::penalize(const std::shared_ptr<core::FlowFile> &flow) {
  const std::chrono::milliseconds penalization_period = process_context_->getProcessorNode()->getPenalizationPeriod();
  logger_->log_info("Penalizing {} for {} at {}", flow->getUUIDStr(), penalization_period, process_context_->getProcessorNode()->getName());
  std::dynamic_pointer_cast<FlowFileImpl>(flow)->penalize(penalization_period);
}

void ProcessSessionImpl::transfer(const std::shared_ptr<core::FlowFile>& flow, const Relationship& relationship) {
  logger_->log_debug("Transferring {} from {} to relationship {}", flow->getUUIDStr(), process_context_->getProcessorNode()->getName(), relationship.getName());
  utils::Identifier uuid = flow->getUUID();
  if (auto it = added_flowfiles_.find(uuid); it != added_flowfiles_.end()) {
    it->second.rel = &*relationships_.insert(relationship).first;
  } else {
    updated_relationships_[uuid] = &*relationships_.insert(relationship).first;
  }
  flow->setDeleted(false);
}

void ProcessSessionImpl::transferToCustomRelationship(const std::shared_ptr<core::FlowFile>& flow, const std::string& relationship_name) {
  transfer(flow, Relationship{relationship_name, relationship_name});
}

void ProcessSessionImpl::write(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback) {
  return write(*flow, callback);
}

void ProcessSessionImpl::write(core::FlowFile &flow, const io::OutputStreamCallback& callback) {
  gsl_ExpectsAudit(updated_flowfiles_.contains(flow.getUUID())
      || added_flowfiles_.contains(flow.getUUID())
      || std::any_of(cloned_flowfiles_.begin(), cloned_flowfiles_.end(), [&flow](const auto& flow_file) { return &flow == flow_file.get(); }));

  std::shared_ptr<ResourceClaim> claim = content_session_->create();

  try {
    auto start_time = std::chrono::steady_clock::now();
    std::shared_ptr<io::BaseStream> stream = content_session_->write(claim);
    // Call the callback to write the content
    if (nullptr == stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for write");
    }
    if (callback(stream) < 0) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
    }

    flow.setSize(stream->size());
    flow.setOffset(0);
    flow.setResourceClaim(claim);

    stream->close();
    std::string details = process_context_->getProcessorNode()->getName() + " modify flow record content " + flow.getUUIDStr();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    provenance_report_->modifyContent(flow, details, duration);
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception during process session write, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session write, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}

void ProcessSessionImpl::writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const char> buffer) {
  writeBuffer(flow_file, as_bytes(buffer));
}
void ProcessSessionImpl::writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const std::byte> buffer) {
  write(flow_file, [buffer](const std::shared_ptr<io::OutputStream>& output_stream) {
    const auto write_status = output_stream->write(buffer);
    return io::isError(write_status) ? -1 : gsl::narrow<int64_t>(write_status);
  });
}

void ProcessSessionImpl::append(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback) {
  gsl_ExpectsAudit(updated_flowfiles_.contains(flow->getUUID())
      || added_flowfiles_.contains(flow->getUUID())
      || std::any_of(cloned_flowfiles_.begin(), cloned_flowfiles_.end(), [&flow](const auto& flow_file) { return flow == flow_file; }));

  std::shared_ptr<ResourceClaim> claim = flow->getResourceClaim();
  if (!claim) {
    // No existed claim for append, we need to create new claim
    return write(flow, callback);
  }

  try {
    auto start_time = std::chrono::steady_clock::now();
    size_t end_offset = flow->getOffset() + flow->getSize();
    std::shared_ptr<io::BaseStream> stream = content_session_->append(claim, end_offset, [&] (const auto& new_claim) {flow->setResourceClaim(new_claim);});
    if (nullptr == stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for append");
    }
    // Call the callback to write the content

    size_t flow_file_size = flow->getSize();
    size_t stream_size_before_callback = stream->size();
    // this prevents an issue if we write, above, with zero length.
    if (stream_size_before_callback > 0)
      stream->seek(stream_size_before_callback);
    if (callback(stream) < 0) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
    }
    flow->setSize(flow_file_size + (stream->size() - stream_size_before_callback));

    std::stringstream details;
    details << process_context_->getProcessorNode()->getName() << " modify flow record content " << flow->getUUIDStr();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    provenance_report_->modifyContent(*flow, details.str(), duration);
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception during process session append, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session append, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}
void ProcessSessionImpl::appendBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const char> buffer) {
  appendBuffer(flow_file, as_bytes(buffer));
}
void ProcessSessionImpl::appendBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const std::byte> buffer) {
  append(flow_file, [buffer](const std::shared_ptr<io::OutputStream>& output_stream) {
    const auto write_status = output_stream->write(buffer);
    return io::isError(write_status) ? -1 : gsl::narrow<int64_t>(write_status);
  });
}

std::shared_ptr<io::InputStream> ProcessSessionImpl::getFlowFileContentStream(const core::FlowFile& flow_file) {
  if (flow_file.getResourceClaim() == nullptr) {
    logger_->log_debug("For {}, no resource claim but size is {}", flow_file.getUUIDStr(), flow_file.getSize());
    if (flow_file.getSize() == 0) {
      return {};
    }
    throw Exception(FILE_OPERATION_EXCEPTION, "No Content Claim existed for read");
  }

  std::shared_ptr<ResourceClaim> claim = flow_file.getResourceClaim();
  std::shared_ptr<io::InputStream> stream = content_session_->read(claim);
  if (nullptr == stream) {
    throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for read");
  }

  return std::make_shared<io::StreamSlice>(stream, flow_file.getOffset(), flow_file.getSize());
}

int64_t ProcessSessionImpl::read(const std::shared_ptr<core::FlowFile>& flow_file, const io::InputStreamCallback& callback) {
  return read(*flow_file, callback);
}

int64_t ProcessSessionImpl::read(const core::FlowFile& flow_file, const io::InputStreamCallback& callback) {
  try {
    auto flow_file_stream = getFlowFileContentStream(flow_file);
    if (!flow_file_stream) {
      return 0;
    }

    auto ret = callback(flow_file_stream);
    if (ret < 0) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
    }
    return ret;
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception {}", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session read");
    throw;
  }
}


int64_t ProcessSessionImpl::readWrite(const std::shared_ptr<core::FlowFile> &flow, const io::InputOutputStreamCallback& callback) {
  gsl_Expects(callback);

  try {
    if (flow->getResourceClaim() == nullptr) {
      logger_->log_debug("For {}, no resource claim but size is {}", flow->getUUIDStr(), flow->getSize());
      if (flow->getSize() == 0) {
        return 0;
      }
      throw Exception(FILE_OPERATION_EXCEPTION, "No Content Claim existed for read");
    }

    std::shared_ptr<ResourceClaim> input_claim = flow->getResourceClaim();
    std::shared_ptr<io::BaseStream> input_stream = content_session_->read(input_claim);
    if (!input_stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for read");
    }
    input_stream->seek(flow->getOffset());

    std::shared_ptr<ResourceClaim> output_claim = content_session_->create();
    std::shared_ptr<io::BaseStream> output_stream = content_session_->write(output_claim);
    if (!output_stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to open flowfile content for write");
    }

    int64_t bytes_written = callback(input_stream, output_stream);
    if (bytes_written < 0) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
    }

    input_stream->close();
    output_stream->close();

    flow->setSize(gsl::narrow<uint64_t>(bytes_written));
    flow->setOffset(0);
    flow->setResourceClaim(output_claim);

    return bytes_written;
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught exception during process session readWrite, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught unknown exception during process session readWrite, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}

detail::ReadBufferResult ProcessSessionImpl::readBuffer(const std::shared_ptr<core::FlowFile>& flow) {
  detail::ReadBufferResult result;
  result.status = read(flow, [&result, this](const std::shared_ptr<io::InputStream>& input_stream) {
    result.buffer.resize(input_stream->size());
    const auto read_status = input_stream->read(result.buffer);
    if (read_status != result.buffer.size()) {
      logger_->log_error("readBuffer: {} bytes were requested from the stream but {} bytes were read. Rolling back.", result.buffer.size(), read_status);
      throw Exception(PROCESSOR_EXCEPTION, "Failed to read the entire FlowFile.");
    }
    return gsl::narrow<int64_t>(read_status);
  });
  return result;
}

void ProcessSessionImpl::importFrom(io::InputStream&& stream, const std::shared_ptr<core::FlowFile> &flow) {
  importFrom(stream, flow);
}
/**
 * Imports a file from the data stream
 * @param stream incoming data stream that contains the data to store into a file
 * @param flow flow file
 *
 */
void ProcessSessionImpl::importFrom(io::InputStream &stream, const std::shared_ptr<core::FlowFile> &flow) {
  const std::shared_ptr<ResourceClaim> claim = content_session_->create();
  const auto max_read = gsl::narrow_cast<size_t>(getpagesize());
  std::vector<std::byte> buffer(max_read);

  try {
    auto start_time = std::chrono::steady_clock::now();
    std::shared_ptr<io::BaseStream> content_stream = content_session_->write(claim);

    if (nullptr == content_stream) {
      throw Exception(FILE_OPERATION_EXCEPTION, "Could not obtain claim for " + claim->getContentFullPath());
    }
    size_t position = 0;
    const auto max_size = stream.size();
    while (position < max_size) {
      const auto read_size = std::min(max_read, max_size - position);
      const auto subbuffer = gsl::make_span(buffer).subspan(0, read_size);
      stream.read(subbuffer);

      content_stream->write(subbuffer);
      position += read_size;
    }
    // Open the source file and stream to the flow file

    flow->setSize(content_stream->size());
    flow->setOffset(0);
    flow->setResourceClaim(claim);

    logger_->log_debug("Import offset {} length {} into content {} for FlowFile UUID {}",
        flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath(), flow->getUUIDStr());

    content_stream->close();
    std::stringstream details;
    details << process_context_->getProcessorNode()->getName() << " modify flow record content " << flow->getUUIDStr();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    provenance_report_->modifyContent(*flow, details.str(), duration);
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception during ProcessSession::importFrom, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during ProcessSession::importFrom, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}

void ProcessSessionImpl::import(const std::string& source, const std::shared_ptr<FlowFile> &flow, bool keepSource, uint64_t offset) {
  std::shared_ptr<ResourceClaim> claim = content_session_->create();
  size_t size = getpagesize();
  std::vector<uint8_t> charBuffer(size);

  try {
    auto start_time = std::chrono::steady_clock::now();
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
        input.seekg(gsl::narrow<std::streamoff>(offset));
        if (!input.good()) {
          logger_->log_error("Seeking to {} failed for file {} (does file/filesystem support seeking?)", offset, source);
          invalidWrite = true;
        }
      }
      while (input.good()) {
        input.read(reinterpret_cast<char*>(charBuffer.data()), gsl::narrow<std::streamsize>(size));
        if (input) {
          if (io::isError(stream->write(charBuffer.data(), size))) {
            invalidWrite = true;
            break;
          }
        } else {
          if (io::isError(stream->write(reinterpret_cast<uint8_t*>(charBuffer.data()), gsl::narrow<size_t>(input.gcount())))) {
            invalidWrite = true;
            break;
          }
        }
      }

      if (!invalidWrite) {
        flow->setSize(stream->size());
        flow->setOffset(0);
        flow->setResourceClaim(claim);

        logger_->log_debug("Import offset {} length {} into content {} for FlowFile UUID {}", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath(),
                           flow->getUUIDStr());

        stream->close();
        input.close();
        if (!keepSource) {
          (void)std::remove(source.c_str());
        }
        std::stringstream details;
        details << process_context_->getProcessorNode()->getName() << " modify flow record content " << flow->getUUIDStr();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        provenance_report_->modifyContent(*flow, details.str(), duration);
      } else {
        stream->close();
        input.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
    }
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception during ProcessSession::import, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during ProcessSession::import, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}

void ProcessSessionImpl::import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, uint64_t offset, char inputDelimiter) {
  std::shared_ptr<ResourceClaim> claim;
  std::shared_ptr<io::BaseStream> stream;
  std::shared_ptr<core::FlowFile> flowFile;

  std::vector<uint8_t> buffer(getpagesize());
  try {
    std::ifstream input{source, std::ios::in | std::ios::binary};
    logger_->log_debug("Opening {}", source);
    if (!input.is_open() || !input.good()) {
      throw Exception(FILE_OPERATION_EXCEPTION, utils::string::join_pack("File Import Error: failed to open file \'", source, "\'"));
    }
    if (offset != 0U) {
      input.seekg(gsl::narrow<std::streamoff>(offset), std::ifstream::beg);
      if (!input.good()) {
        logger_->log_error("Seeking to {} failed for file {} (does file/filesystem support seeking?)", offset, source);
        throw Exception(FILE_OPERATION_EXCEPTION, utils::string::join_pack("File Import Error: Couldn't seek to offset ", std::to_string(offset)));
      }
    }
    while (input.good()) {
      input.read(reinterpret_cast<char*>(buffer.data()), gsl::narrow<std::streamsize>(buffer.size()));
      std::streamsize read = input.gcount();
      if (read < 0) {
        throw Exception(FILE_OPERATION_EXCEPTION, "std::ifstream::gcount returned negative value");
      }
      if (read == 0) {
        logger_->log_trace("Finished reading input {}", source);
        break;
      } else {
        logger_->log_trace("Read input of {}", read);
      }
      uint8_t* begin = buffer.data();
      uint8_t* end = begin + read;
      while (true) {
        auto start_time = std::chrono::steady_clock::now();
        uint8_t* delimiterPos = std::find(begin, end, static_cast<uint8_t>(inputDelimiter));
        const auto len = gsl::narrow<size_t>(delimiterPos - begin);

        logger_->log_trace("Read input of {} length is {} is at end? {}", read, len, delimiterPos == end);
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
          start_time = std::chrono::steady_clock::now();
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
        logger_->log_debug("Import offset {} length {} into content {}, FlowFile UUID {}",
            flowFile->getOffset(), flowFile->getSize(), flowFile->getResourceClaim()->getContentFullPath(), flowFile->getUUIDStr());
        stream->close();
        std::string details = process_context_->getProcessorNode()->getName() + " modify flow record content " + flowFile->getUUIDStr();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
        provenance_report_->modifyContent(*flowFile, details, duration);
        flows.push_back(flowFile);

        /* Reset these to start processing the next FlowFile with a clean slate */
        flowFile.reset();
        stream.reset();
        claim.reset();

        /* Skip delimiter */
        begin = delimiterPos + 1;
      }
    }
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception during ProcessSession::import, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during ProcessSession::import, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}

void ProcessSessionImpl::import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, bool keepSource, uint64_t offset, char inputDelimiter) {
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
  logger_->log_trace("Closed input {}, keeping source ? {}", source, keepSource);
  if (!keepSource) {
    (void)std::remove(source.c_str());
  }
}

bool ProcessSessionImpl::exportContent(const std::string &destination, const std::string &tmpFile, const std::shared_ptr<core::FlowFile> &flow, bool /*keepContent*/) {
  logger_->log_debug("Exporting content of {} to {}", flow->getUUIDStr(), destination);

  ProcessSessionReadCallback cb(tmpFile, destination, logger_);
  read(flow, std::ref(cb));

  logger_->log_info("Committing {}", destination);
  bool commit_ok = cb.commit();

  if (commit_ok) {
    logger_->log_info("Commit OK.");
  } else {
    logger_->log_error("Commit of {} to {} failed!", flow->getUUIDStr(), destination);
  }
  return commit_ok;
}

bool ProcessSessionImpl::exportContent(const std::string &destination, const std::shared_ptr<core::FlowFile> &flow, bool keepContent) {
  utils::Identifier tmpFileUuid = id_generator_->generate();
  std::stringstream tmpFileSs;
  tmpFileSs << destination << "." << tmpFileUuid.to_string();
  std::string tmpFileName = tmpFileSs.str();

  return exportContent(destination, tmpFileName, flow, keepContent);
}

void ProcessSessionImpl::stash(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) {
  logger_->log_debug("Stashing content from {} to key {}", flow->getUUIDStr(), key);

  auto claim = flow->getResourceClaim();
  if (!claim) {
    logger_->log_warn("Attempted to stash content of record {} when "
                      "there is no resource claim",
                      flow->getUUIDStr());
    return;
  }

  // Stash the claim
  flow->setStashClaim(key, claim);

  // Clear current claim
  flow->clearResourceClaim();
}

void ProcessSessionImpl::restore(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) {
  logger_->log_info("Restoring content to {} from key {}", flow->getUUIDStr(), key);

  // Restore the claim
  if (!flow->hasStashClaim(key)) {
    logger_->log_warn("Requested restore to record {} from unknown key {}", flow->getUUIDStr(), key);
    return;
  }

  // Disown current claim if existing
  if (flow->getResourceClaim()) {
    logger_->log_warn("Restoring stashed content of record {} from key {} when there is "
                      "existing content; existing content will be overwritten",
                      flow->getUUIDStr(), key);
  }

  // Restore the claim
  auto stashClaim = flow->getStashClaim(key);
  flow->setResourceClaim(stashClaim);
  flow->clearStashClaim(key);
}

ProcessSessionImpl::RouteResult ProcessSessionImpl::routeFlowFile(const std::shared_ptr<FlowFile> &record, const std::function<void(const FlowFile&, const Relationship&)>& transfer_callback) {
  if (record->isDeleted()) {
    return RouteResult::Ok_Deleted;
  }
  utils::Identifier uuid = record->getUUID();
  Relationship relationship;
  if (auto it = updated_relationships_.find(uuid); it != updated_relationships_.end()) {
    gsl_Expects(it->second);
    relationship = *it->second;
  } else if (auto new_it = added_flowfiles_.find(uuid); new_it != added_flowfiles_.end() && new_it->second.rel) {
    relationship = *new_it->second.rel;
  } else {
    return RouteResult::Error_NoRelationship;
  }
  // Find the relationship, we need to find the connections for that relationship
  const auto connections = process_context_->getProcessorNode()->getOutGoingConnections(relationship.getName());
  if (connections.empty()) {
    // No connection
    if (!process_context_->getProcessorNode()->isAutoTerminated(relationship)) {
      // Not autoterminate, we should have the connect
      std::string message = "Connect empty for non auto terminated relationship " + relationship.getName();
      throw Exception(PROCESS_SESSION_EXCEPTION, message);
    } else {
      // Autoterminated
      remove(record);
      transfer_callback(*record, relationship);
      return RouteResult::Ok_AutoTerminated;
    }
  } else {
    // We connections, clone the flow and assign the connection accordingly
    for (auto itConnection = connections.begin(); itConnection != connections.end(); ++itConnection) {
      auto connection = *itConnection;
      if (itConnection == connections.begin()) {
        // First connection which the flow need be routed to
        record->setConnection(connection);
        transfer_callback(*record, relationship);
      } else {
        // Clone the flow file and route to the connection
        std::shared_ptr<core::FlowFile> cloneRecord = this->cloneDuringTransfer(*record);
        if (cloneRecord) {
          cloneRecord->setConnection(connection);
          transfer_callback(*cloneRecord, relationship);
        } else {
          throw Exception(PROCESS_SESSION_EXCEPTION, "Can not clone the flow for transfer " + record->getUUIDStr());
        }
      }
    }
  }
  return RouteResult::Ok_Routed;
}

void ProcessSessionImpl::commit() {
  const auto commit_start_time = std::chrono::steady_clock::now();
  try {
    std::unordered_map<std::string, TransferMetrics> transfers;
      auto increaseTransferMetrics = [&](const FlowFile& record, const Relationship& relationship) {
      ++transfers[relationship.getName()].transfer_count;
      transfers[relationship.getName()].transfer_size += record.getSize();
    };
    // First we clone the flow record based on the transferred relationship for updated flow record
    for (auto && it : updated_flowfiles_) {
      auto record = it.second.modified;
      if (routeFlowFile(record, increaseTransferMetrics) == RouteResult::Error_NoRelationship) {
        // Can not find relationship for the flow
        throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the updated flow " + record->getUUIDStr());
      }
    }

    // Do the same thing for added flow file
    for (const auto& it : added_flowfiles_) {
      auto record = it.second.flow_file;
      if (routeFlowFile(record, increaseTransferMetrics) == RouteResult::Error_NoRelationship) {
        // Can not find relationship for the flow
        throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the added flow " + record->getUUIDStr());
      }
    }

    std::map<Connectable*, std::vector<std::shared_ptr<FlowFile>>> connectionQueues;

    Connectable* connection = nullptr;
    // Complete process the added and update flow files for the session, send the flow file to its queue
    for (const auto &it : updated_flowfiles_) {
      auto record = it.second.modified;
      logger_->log_trace("See {} in {}", record->getUUIDStr(), "updated_flowfiles_");
      if (record->isDeleted()) {
        continue;
      }

      connection = record->getConnection();
      if ((connection) != nullptr) {
        connectionQueues[connection].push_back(record);
      }
    }
    for (const auto &it : added_flowfiles_) {
      auto record = it.second.flow_file;
      logger_->log_trace("See {} in {}", record->getUUIDStr(), "added_flowfiles_");
      if (record->isDeleted()) {
        continue;
      }
      connection = record->getConnection();
      if ((connection) != nullptr) {
        connectionQueues[connection].push_back(record);
      }
    }
    // Process the clone flow files
    for (const auto &record : cloned_flowfiles_) {
      logger_->log_trace("See {} in {}", record->getUUIDStr(), "cloned_flowfiles_");
      if (record->isDeleted()) {
        continue;
      }
      connection = record->getConnection();
      if ((connection) != nullptr) {
        connectionQueues[connection].push_back(record);
      }
    }

    for (const auto& record : deleted_flowfiles_) {
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

    if (stateManager_ && !stateManager_->commit()) {
      throw Exception(PROCESS_SESSION_EXCEPTION, "State manager commit failed.");
    }

    persistFlowFilesBeforeTransfer(connectionQueues, updated_flowfiles_);

    for (auto& cq : connectionQueues) {
      auto connection_from_queue = dynamic_cast<Connection*>(cq.first);
      if (connection_from_queue) {
        connection_from_queue->multiPut(cq.second);
      } else {
        for (auto& file : cq.second) {
          cq.first->put(file);
        }
      }
    }

    if (metrics_) {
      for (const auto& [relationship_name, transfer_metrics] : transfers) {
        metrics_->transferred_bytes() += transfer_metrics.transfer_size;
        metrics_->transferred_flow_files() += transfer_metrics.transfer_count;
        metrics_->increaseRelationshipTransferCount(relationship_name, transfer_metrics.transfer_count);
      }
    }

    // All done
    updated_flowfiles_.clear();
    added_flowfiles_.clear();
    cloned_flowfiles_.clear();
    deleted_flowfiles_.clear();

    updated_relationships_.clear();
    relationships_.clear();
    // persistent the provenance report
    this->provenance_report_->commit();
    logger_->log_debug("ProcessSession committed for {}", process_context_->getProcessorNode()->getName());
    if (metrics_)
      metrics_->addLastSessionCommitRuntime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - commit_start_time));
  } catch (const std::exception& exception) {
    logger_->log_debug("Caught Exception during process session commit, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session commit, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}

void ProcessSessionImpl::rollback() {
  // new FlowFiles are only persisted during commit
  // no need to delete them here
  std::map<Connectable*, std::vector<std::shared_ptr<FlowFile>>> connectionQueues;

  try {
    // Requeue the snapshot of the flowfile back
    for (const auto &it : updated_flowfiles_) {
      auto flowFile = it.second.modified;
      // restore flowFile to original state
      *flowFile = *it.second.snapshot;
      penalize(flowFile);
      logger_->log_debug("ProcessSession rollback for {}, record {}, to connection {}",
          process_context_->getProcessorNode()->getName(),
          flowFile->getUUIDStr(),
          flowFile->getConnection()->getName());
      connectionQueues[flowFile->getConnection()].push_back(flowFile);
    }

    for (const auto& record : deleted_flowfiles_) {
      record->setDeleted(false);
    }

    // put everything back where it came from
    for (auto& cq : connectionQueues) {
      auto connection = dynamic_cast<Connection*>(cq.first);
      if (connection) {
        connection->multiPut(cq.second);
      } else {
        for (auto& flow : cq.second) {
          cq.first->put(flow);
        }
      }
    }

    content_session_->rollback();

    if (stateManager_ && !stateManager_->rollback()) {
      throw Exception(PROCESS_SESSION_EXCEPTION, "State manager rollback failed.");
    }

    cloned_flowfiles_.clear();
    added_flowfiles_.clear();
    updated_flowfiles_.clear();
    deleted_flowfiles_.clear();
    relationships_.clear();
    logger_->log_warn("ProcessSession rollback for {} executed", process_context_->getProcessorNode()->getName());
  } catch (const std::exception& exception) {
    logger_->log_warn("Caught Exception during process session rollback, type: {}, what: {}", typeid(exception).name(), exception.what());
    throw;
  } catch (...) {
    logger_->log_warn("Caught Exception during process session rollback, type: {}", getCurrentExceptionTypeName());
    throw;
  }
}

nonstd::expected<void, std::exception_ptr> ProcessSessionImpl::rollbackNoThrow() noexcept {
  try {
    rollback();
    return {};
  } catch(...) {
    return nonstd::make_unexpected(std::current_exception());
  }
}

void ProcessSessionImpl::persistFlowFilesBeforeTransfer(
    std::map<Connectable*, std::vector<std::shared_ptr<core::FlowFile> > >& transactionMap,
    const std::map<utils::Identifier, FlowFileUpdate>& modifiedFlowFiles) {

  std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>> flowData;

  auto flowFileRepo = process_context_->getFlowFileRepository();
  auto contentRepo = process_context_->getContentRepository();

  enum class Type {
    Dropped, Transferred
  };

  auto forEachFlowFile = [&] (Type type, auto fn) {
    for (auto& [target, flows] : transactionMap) {
      const auto connection = dynamic_cast<Connection*>(target);
      const bool shouldDropEmptyFiles = connection && connection->getDropEmptyFlowFiles();
      for (auto &ff : flows) {
        auto snapshotIt = modifiedFlowFiles.find(ff->getUUID());
        auto original = snapshotIt != modifiedFlowFiles.end() ? snapshotIt->second.snapshot : nullptr;
        if (shouldDropEmptyFiles && ff->getSize() == 0) {
          // the receiver will drop this FF
          if (type == Type::Dropped) {
            fn(ff, original);
          }
        } else {
          if (type == Type::Transferred) {
            fn(ff, original);
          }
        }
      }
    }
  };

  // collect serialized flowfiles
  forEachFlowFile(Type::Transferred, [&] (auto& ff, auto& /*original*/) {
    auto stream = std::make_unique<io::BufferStream>();
    std::dynamic_pointer_cast<FlowFileRecord>(ff)->Serialize(*stream);

    flowData.emplace_back(ff->getUUIDStr(), std::move(stream));
  });

  // increment on behalf of the to be persisted instance
  forEachFlowFile(Type::Transferred, [&] (auto& ff, auto& /*original*/) {
    if (auto claim = ff->getResourceClaim())
      claim->increaseFlowFileRecordOwnedCount();
  });

  if (!flowFileRepo->MultiPut(flowData)) {
    logger_->log_error("Failed execute multiput on FF repo!");
    // decrement on behalf of the failed persisted instance
    forEachFlowFile(Type::Transferred, [&] (auto& ff, auto& /*original*/) {
      if (auto claim = ff->getResourceClaim())
        claim->decreaseFlowFileRecordOwnedCount();
    });
    throw Exception(PROCESS_SESSION_EXCEPTION, "Failed to put flowfiles to repository");
  }

  // decrement on behalf of the overridden instance if any
  forEachFlowFile(Type::Transferred, [&] (auto& ff, auto& original) {
    if (auto original_claim = original ? original->getResourceClaim() : nullptr) {
      original_claim->decreaseFlowFileRecordOwnedCount();
    }
    ff->setStoredToRepository(true);
  });

  forEachFlowFile(Type::Dropped, [&] (auto& ff, auto& original) {
    // the receiver promised to drop this FF, no need for it anymore
    if (ff->isStored() && flowFileRepo->Delete(ff->getUUIDStr())) {
      // original must be non-null since this flowFile is already stored in the repos ->
      // must have come from a session->get()
      gsl_Assert(original);
      ff->setStoredToRepository(false);
    }
  });
}

void ProcessSessionImpl::ensureNonNullResourceClaim(
    const std::map<Connectable*, std::vector<std::shared_ptr<core::FlowFile>>> &transactionMap) {
  for (auto& transaction : transactionMap) {
    for (auto& flowFile : transaction.second) {
      auto claim = flowFile->getResourceClaim();
      if (!claim) {
        logger_->log_debug("Processor {} ({}) did not create a ResourceClaim, creating an empty one",
                           process_context_->getProcessorNode()->getUUIDStr(),
                           process_context_->getProcessorNode()->getName());
        io::BufferStream emptyBufferStream;
        write(flowFile, OutputStreamPipe{emptyBufferStream});
      }
    }
  }
}

std::shared_ptr<core::FlowFile> ProcessSessionImpl::get() {
  const auto first = process_context_->getProcessorNode()->pickIncomingConnection();

  if (first == nullptr) {
    logger_->log_trace("Get is null for {}", process_context_->getProcessorNode()->getName());
    return nullptr;
  }

  auto current = dynamic_cast<Connection*>(first);
  if (!current) {
    logger_->log_error("The incoming connection [{}] of the processor [{}] \"{}\" is not actually a Connection.",
                       first->getUUIDStr(), process_context_->getProcessorNode()->getUUIDStr(), process_context_->getProcessorNode()->getName());
    return {};
  }

  do {
    std::set<std::shared_ptr<core::FlowFile> > expired;
    std::shared_ptr<core::FlowFile> ret = current->poll(expired);
    if (!expired.empty()) {
      // Remove expired flow record
      for (const auto& record : expired) {
        std::stringstream details;
        details << process_context_->getProcessorNode()->getName() << " expire flow record " << record->getUUIDStr();
        provenance_report_->expire(*record, details.str());
        // there is no rolling back expired FlowFiles
        if (record->isStored() && process_context_->getFlowFileRepository()->Delete(record->getUUIDStr())) {
          record->setStoredToRepository(false);
        }
      }
    }
    if (ret) {
      // add the flow record to the current process session update map
      ret->setDeleted(false);
      std::shared_ptr<FlowFile> snapshot = std::make_shared<FlowFileRecordImpl>();
      *snapshot = *ret;
      logger_->log_debug("Create Snapshot FlowFile with UUID {}", snapshot->getUUIDStr());
      utils::Identifier uuid = ret->getUUID();
      updated_flowfiles_[uuid] = {ret, snapshot};
      auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
      if (flow_version != nullptr) {
        ret->setAttribute(SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
      }
      return ret;
    }
    current = dynamic_cast<Connection*>(process_context_->getProcessorNode()->pickIncomingConnection());
  } while (current != nullptr && current != first);

  return nullptr;
}

void ProcessSessionImpl::flushContent() {
  content_session_->commit();
}

bool ProcessSessionImpl::outgoingConnectionsFull(const std::string& relationship) {
  std::set<Connectable*> connections = process_context_->getProcessorNode()->getOutGoingConnections(relationship);
  Connection * connection = nullptr;
  for (const auto conn : connections) {
    connection = dynamic_cast<Connection*>(conn);
    if (connection && connection->backpressureThresholdReached()) {
      return true;
    }
  }
  return false;
}

bool ProcessSessionImpl::existsFlowFileInRelationship(const Relationship &relationship) {
  return std::any_of(updated_relationships_.begin(), updated_relationships_.end(),
      [&](const auto& key_value_pair) {
        return key_value_pair.second && relationship == *key_value_pair.second;
  }) || std::any_of(added_flowfiles_.begin(), added_flowfiles_.end(),
      [&](const auto& key_value_pair) {
        return key_value_pair.second.rel && relationship == *key_value_pair.second.rel;
  });
}

bool ProcessSessionImpl::hasBeenTransferred(const core::FlowFile &flow) const {
  return (updated_relationships_.contains(flow.getUUID()) && updated_relationships_.at(flow.getUUID()) != nullptr) ||
    (added_flowfiles_.contains(flow.getUUID()) && added_flowfiles_.at(flow.getUUID()).rel != nullptr);
}

}  // namespace org::apache::nifi::minifi::core
