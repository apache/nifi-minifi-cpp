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
#include <sys/time.h>
#include <time.h>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <chrono>
#include <thread>
#include <iostream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::shared_ptr<core::FlowFile> ProcessSession::create() {
  std::map<std::string, std::string> empty;

  std::shared_ptr<core::FlowFile> record = std::make_shared<FlowFileRecord>(process_context_->getFlowFileRepository(), process_context_->getContentRepository(), empty);

  _addedFlowFiles[record->getUUIDStr()] = record;
  logger_->log_debug("Create FlowFile with UUID %s", record->getUUIDStr().c_str());
  std::stringstream details;
  details << process_context_->getProcessorNode().getName() << " creates flow record " << record->getUUIDStr();
  provenance_report_->create(record, details.str());

  return record;
}

void ProcessSession::add(std::shared_ptr<core::FlowFile> &record) {
  _addedFlowFiles[record->getUUIDStr()] = record;
}

std::shared_ptr<core::FlowFile> ProcessSession::create(std::shared_ptr<core::FlowFile> &&parent) {
  std::map<std::string, std::string> empty;
  std::shared_ptr<core::FlowFile> record = std::make_shared<FlowFileRecord>(process_context_->getFlowFileRepository(), process_context_->getContentRepository(), empty);

  if (record) {
    _addedFlowFiles[record->getUUIDStr()] = record;
    logger_->log_debug("Create FlowFile with UUID %s", record->getUUIDStr().c_str());
  }

  if (record) {
    // Copy attributes
    std::map<std::string, std::string> parentAttributes = parent->getAttributes();
    std::map<std::string, std::string>::iterator it;
    for (it = parentAttributes.begin(); it != parentAttributes.end(); it++) {
      if (it->first == FlowAttributeKey(ALTERNATE_IDENTIFIER) || it->first == FlowAttributeKey(DISCARD_REASON) || it->first == FlowAttributeKey(UUID))
        // Do not copy special attributes from parent
        continue;
      record->setAttribute(it->first, it->second);
    }
    record->setLineageStartDate(parent->getlineageStartDate());
    record->setLineageIdentifiers(parent->getlineageIdentifiers());
    parent->getlineageIdentifiers().insert(parent->getUUIDStr());
  }
  return record;
}

std::shared_ptr<core::FlowFile> ProcessSession::clone(std::shared_ptr<core::FlowFile> &parent) {
  std::shared_ptr<core::FlowFile> record = this->create(parent);
  if (record) {
    // Copy Resource Claim
    std::shared_ptr<ResourceClaim> parent_claim = parent->getResourceClaim();
    record->setResourceClaim(parent_claim);
    if (parent_claim != nullptr) {
      record->setOffset(parent->getOffset());
      record->setSize(parent->getSize());
      record->getResourceClaim()->increaseFlowFileRecordOwnedCount();
    }
    provenance_report_->clone(parent, record);
  }
  return record;
}

std::shared_ptr<core::FlowFile> ProcessSession::cloneDuringTransfer(std::shared_ptr<core::FlowFile> &parent) {
  std::map<std::string, std::string> empty;
  std::shared_ptr<core::FlowFile> record = std::make_shared<FlowFileRecord>(process_context_->getFlowFileRepository(), process_context_->getContentRepository(), empty);

  if (record) {
    this->_clonedFlowFiles[record->getUUIDStr()] = record;
    logger_->log_debug("Clone FlowFile with UUID %s during transfer", record->getUUIDStr().c_str());
    // Copy attributes
    std::map<std::string, std::string> parentAttributes = parent->getAttributes();
    std::map<std::string, std::string>::iterator it;
    for (it = parentAttributes.begin(); it != parentAttributes.end(); it++) {
      if (it->first == FlowAttributeKey(ALTERNATE_IDENTIFIER) || it->first == FlowAttributeKey(DISCARD_REASON) || it->first == FlowAttributeKey(UUID))
        // Do not copy special attributes from parent
        continue;
      record->setAttribute(it->first, it->second);
    }
    record->setLineageStartDate(parent->getlineageStartDate());

    record->setLineageIdentifiers(parent->getlineageIdentifiers());
    record->getlineageIdentifiers().insert(parent->getUUIDStr());

    // Copy Resource Claim
    std::shared_ptr<ResourceClaim> parent_claim = parent->getResourceClaim();
    record->setResourceClaim(parent_claim);
    if (parent_claim != nullptr) {
      record->setOffset(parent->getOffset());
      record->setSize(parent->getSize());
      record->getResourceClaim()->increaseFlowFileRecordOwnedCount();
    }
    provenance_report_->clone(parent, record);
  }

  return record;
}

std::shared_ptr<core::FlowFile> ProcessSession::clone(std::shared_ptr<core::FlowFile> &parent, int64_t offset, int64_t size) {
  std::shared_ptr<core::FlowFile> record = this->create(parent);
  if (record) {
    if (parent->getResourceClaim()) {
      if ((offset + size) > parent->getSize()) {
        // Set offset and size
        logger_->log_error("clone offset %d and size %d exceed parent size %d", offset, size, parent->getSize());
        // Remove the Add FlowFile for the session
        std::map<std::string, std::shared_ptr<core::FlowFile> >::iterator it = this->_addedFlowFiles.find(record->getUUIDStr());
        if (it != this->_addedFlowFiles.end())
          this->_addedFlowFiles.erase(record->getUUIDStr());
        return nullptr;
      }
      record->setOffset(parent->getOffset() + parent->getOffset());
      record->setSize(size);
      // Copy Resource Claim
      std::shared_ptr<ResourceClaim> parent_claim = parent->getResourceClaim();
      record->setResourceClaim(parent_claim);
      if (parent_claim != nullptr) {
        record->getResourceClaim()->increaseFlowFileRecordOwnedCount();
      }
    }
    provenance_report_->clone(parent, record);
  }
  return record;
}

void ProcessSession::remove(std::shared_ptr<core::FlowFile> &flow) {
  flow->setDeleted(true);
  _deletedFlowFiles[flow->getUUIDStr()] = flow;
  std::string reason = process_context_->getProcessorNode().getName() + " drop flow record " + flow->getUUIDStr();
  provenance_report_->drop(flow, reason);
}

void ProcessSession::remove(std::shared_ptr<core::FlowFile> &&flow) {
  flow->setDeleted(true);
  _deletedFlowFiles[flow->getUUIDStr()] = flow;
  std::string reason = process_context_->getProcessorNode().getName() + " drop flow record " + flow->getUUIDStr();
  provenance_report_->drop(flow, reason);
}

void ProcessSession::putAttribute(std::shared_ptr<core::FlowFile> &flow, std::string key, std::string value) {
  flow->setAttribute(key, value);
  std::stringstream details;
  details << process_context_->getProcessorNode().getName() << " modify flow record " << flow->getUUIDStr() << " attribute " << key << ":" << value;
  provenance_report_->modifyAttributes(flow, details.str());
}

void ProcessSession::removeAttribute(std::shared_ptr<core::FlowFile> &flow, std::string key) {
  flow->removeAttribute(key);
  std::stringstream details;
  details << process_context_->getProcessorNode().getName() << " remove flow record " << flow->getUUIDStr() << " attribute " + key;
  provenance_report_->modifyAttributes(flow, details.str());
}

void ProcessSession::putAttribute(std::shared_ptr<core::FlowFile> &&flow, std::string key, std::string value) {
  flow->setAttribute(key, value);
  std::stringstream details;
  details << process_context_->getProcessorNode().getName() << " modify flow record " << flow->getUUIDStr() << " attribute " << key << ":" << value;
  provenance_report_->modifyAttributes(flow, details.str());
}

void ProcessSession::removeAttribute(std::shared_ptr<core::FlowFile> &&flow, std::string key) {
  flow->removeAttribute(key);
  std::stringstream details;
  details << process_context_->getProcessorNode().getName() << " remove flow record " << flow->getUUIDStr() << " attribute " << key;
  provenance_report_->modifyAttributes(flow, details.str());
}

void ProcessSession::penalize(std::shared_ptr<core::FlowFile> &flow) {
  flow->setPenaltyExpiration(getTimeMillis() + process_context_->getProcessorNode().getPenalizationPeriodMsec());
}

void ProcessSession::penalize(std::shared_ptr<core::FlowFile> &&flow) {
  flow->setPenaltyExpiration(getTimeMillis() + process_context_->getProcessorNode().getPenalizationPeriodMsec());
}

void ProcessSession::transfer(std::shared_ptr<core::FlowFile> &flow, Relationship relationship) {
  _transferRelationship[flow->getUUIDStr()] = relationship;
}

void ProcessSession::transfer(std::shared_ptr<core::FlowFile> &&flow, Relationship relationship) {
  _transferRelationship[flow->getUUIDStr()] = relationship;
}

void ProcessSession::write(std::shared_ptr<core::FlowFile> &flow, OutputStreamCallback *callback) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(process_context_->getContentRepository());

  try {
    uint64_t startTime = getTimeMillis();
    claim->increaseFlowFileRecordOwnedCount();
//    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->write(claim);
    // Call the callback to write the content
    if (nullptr == stream) {
      rollback();
      return;
    }
    if (callback->process(stream) < 0) {
      rollback();
      return;
    }

    flow->setSize(stream->getSize());
    flow->setOffset(0);
    std::shared_ptr<ResourceClaim> flow_claim = flow->getResourceClaim();
    if (flow_claim != nullptr) {
      // Remove the old claim
      flow_claim->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    flow->setResourceClaim(claim);

    /*
     logger_->log_debug("Write offset %d length %d into content %s for FlowFile UUID %s",
     flow->_offset, flow->_size, flow->_claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str()); */
    stream->closeStream();
    std::stringstream details;
    details << process_context_->getProcessorNode().getName() << " modify flow record content " << flow->getUUIDStr();
    uint64_t endTime = getTimeMillis();
    provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
  } catch (std::exception &exception) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception during process session write");
    throw;
  }
}

void ProcessSession::write(std::shared_ptr<core::FlowFile> &&flow, OutputStreamCallback *callback) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(process_context_->getContentRepository());
  try {
    uint64_t startTime = getTimeMillis();
    claim->increaseFlowFileRecordOwnedCount();
    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->write(claim);
    if (nullptr == stream) {
      rollback();
      return;
    }
    // Call the callback to write the content
    if (callback->process(stream) < 0) {
      rollback();
      return;
    }
    flow->setSize(stream->getSize());
    flow->setOffset(0);
    std::shared_ptr<ResourceClaim> flow_claim = flow->getResourceClaim();
    if (flow_claim != nullptr) {
      // Remove the old claim
      flow_claim->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    flow->setResourceClaim(claim);

    std::stringstream details;
    details << process_context_->getProcessorNode().getName() << " modify flow record content " << flow->getUUIDStr();
    uint64_t endTime = getTimeMillis();
    provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
  } catch (std::exception &exception) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception during process session write");
    throw;
  }
}

void ProcessSession::append(std::shared_ptr<core::FlowFile> &&flow, OutputStreamCallback *callback) {
  std::shared_ptr<ResourceClaim> claim = nullptr;

  if (flow->getResourceClaim() == nullptr) {
    // No existed claim for append, we need to create new claim
    return write(flow, callback);
  }

  claim = flow->getResourceClaim();

  try {
    uint64_t startTime = getTimeMillis();
    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->write(claim);
    if (nullptr == stream) {
      rollback();
      return;
    }
    // Call the callback to write the content
    size_t oldPos = stream->getSize();
    stream->seek(oldPos + 1);
    if (callback->process(stream) < 0) {
      rollback();
      return;
    }
    uint64_t appendSize = stream->getSize() - oldPos;
    flow->setSize(flow->getSize() + appendSize);
    std::stringstream details;
    details << process_context_->getProcessorNode().getName() << " modify flow record content " << flow->getUUIDStr();
    uint64_t endTime = getTimeMillis();
    provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session append");
    throw;
  }
}

void ProcessSession::append(std::shared_ptr<core::FlowFile> &flow, OutputStreamCallback *callback) {
  std::shared_ptr<ResourceClaim> claim = nullptr;

  if (flow->getResourceClaim() == nullptr) {
    // No existed claim for append, we need to create new claim
    return write(flow, callback);
  }

  claim = flow->getResourceClaim();

  try {
    uint64_t startTime = getTimeMillis();
    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->write(claim);
    if (nullptr == stream) {
      rollback();
      return;
    }
    // Call the callback to write the content
    size_t oldPos = stream->getSize();
    stream->seek(oldPos + 1);
    if (callback->process(stream) < 0) {
      rollback();
      return;
    }
    uint64_t appendSize = stream->getSize() - oldPos;
    flow->setSize(flow->getSize() + appendSize);

    std::stringstream details;
    details << process_context_->getProcessorNode().getName() << " modify flow record content " << flow->getUUIDStr();
    uint64_t endTime = getTimeMillis();
    provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session append");
    throw;
  }
}

void ProcessSession::read(std::shared_ptr<core::FlowFile> &flow, InputStreamCallback *callback) {
  try {
    std::shared_ptr<ResourceClaim> claim = nullptr;

    if (flow->getResourceClaim() == nullptr) {
      // No existed claim for read, we throw exception
      throw Exception(FILE_OPERATION_EXCEPTION, "No Content Claim existed for read");
    }

    claim = flow->getResourceClaim();

    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->read(claim);

    if (nullptr == stream) {
      rollback();
      return;
    }

    stream->seek(flow->getOffset());

    if (callback->process(stream) < 0) {
      rollback();
      return;
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session read");
    throw;
  }
}

void ProcessSession::read(std::shared_ptr<core::FlowFile> &&flow, InputStreamCallback *callback) {
  try {
    std::shared_ptr<ResourceClaim> claim = nullptr;

    if (flow->getResourceClaim() == nullptr) {
      // No existed claim for read, we throw exception
      throw Exception(FILE_OPERATION_EXCEPTION, "No Content Claim existed for read");
    }

    claim = flow->getResourceClaim();
    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->read(claim);

    if (nullptr == stream) {
      rollback();
      return;
    }
    stream->seek(flow->getOffset());

    if (callback->process(stream) < 0) {
      rollback();
      return;
    }
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session read");
    throw;
  }
}

/**
 * Imports a file from the data stream
 * @param stream incoming data stream that contains the data to store into a file
 * @param flow flow file
 *
 */
void ProcessSession::importFrom(io::DataStream &stream, std::shared_ptr<core::FlowFile> &&flow) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(process_context_->getContentRepository());
  int max_read = getpagesize();
  std::vector<uint8_t> charBuffer;
  charBuffer.resize(max_read);

  try {
    uint64_t startTime = getTimeMillis();
    claim->increaseFlowFileRecordOwnedCount();
    std::shared_ptr<io::BaseStream> content_stream = process_context_->getContentRepository()->write(claim);

    if (nullptr == content_stream) {
      logger_->log_debug("Could not obtain claim for %s", claim->getContentFullPath());
      rollback();
      return;
    }
    size_t position = 0;
    const size_t max_size = stream.getSize();
    size_t read_size = max_read;
    while (position < max_size) {
      if ((max_size - position) > max_read) {
        read_size = max_read;
      } else {
        read_size = max_size - position;
      }
      charBuffer.clear();
      stream.readData(charBuffer, read_size);

      content_stream->write(charBuffer.data(), read_size);
      position += read_size;
    }
    // Open the source file and stream to the flow file

    flow->setSize(content_stream->getSize());
    flow->setOffset(0);
    if (flow->getResourceClaim() != nullptr) {
      // Remove the old claim
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    flow->setResourceClaim(claim);

    logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath().c_str(),
                       flow->getUUIDStr().c_str());

    content_stream->closeStream();
    std::stringstream details;
    details << process_context_->getProcessorNode().getName() << " modify flow record content " << flow->getUUIDStr();
    uint64_t endTime = getTimeMillis();
    provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
  } catch (std::exception &exception) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception during process session write");
    throw;
  }
}

void ProcessSession::import(std::string source, std::shared_ptr<core::FlowFile> &flow, bool keepSource, uint64_t offset) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(process_context_->getContentRepository());
  char *buf = NULL;
  int size = 4096;
  buf = new char[size];

  try {
    //  std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    std::ifstream input;
    input.open(source.c_str(), std::fstream::in | std::fstream::binary);
    claim->increaseFlowFileRecordOwnedCount();
    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->write(claim);
    if (nullptr == stream) {
      rollback();
      return;
    }
    if (input.is_open()) {
      // Open the source file and stream to the flow file
      input.seekg(offset);
      bool invalidWrite = false;
      while (input.good()) {
        input.read(buf, size);
        if (input) {
          if (stream->write(reinterpret_cast<uint8_t*>(buf), size) < 0) {
            invalidWrite = true;
            break;
          }
        } else {
          if (stream->write(reinterpret_cast<uint8_t*>(buf), input.gcount()) < 0) {
            invalidWrite = true;
            break;
          }
        }
      }

      if (!invalidWrite) {
        flow->setSize(stream->getSize());
        flow->setOffset(0);
        if (flow->getResourceClaim() != nullptr) {
          // Remove the old claim
          flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);

        logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath().c_str(),
                           flow->getUUIDStr().c_str());

        stream->closeStream();
        input.close();
        if (!keepSource)
          std::remove(source.c_str());
        std::stringstream details;
        details << process_context_->getProcessorNode().getName() << " modify flow record content " << flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
      } else {
        stream->closeStream();
        input.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
    }

    delete[] buf;
  } catch (std::exception &exception) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception %s", exception.what());
    delete[] buf;
    throw;
  } catch (...) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception during process session write");
    delete[] buf;
    throw;
  }
}

void ProcessSession::import(std::string source, std::vector<std::shared_ptr<FlowFileRecord>> flows, bool keepSource, uint64_t offset, char inputDelimiter) {
  std::shared_ptr<ResourceClaim> claim;

  std::shared_ptr<FlowFileRecord> flowFile;

  char *buf = NULL;
  int size = 4096;
  buf = new char[size];

  try {
    // Open the input file and seek to the appropriate location.
    std::ifstream input;
    logger_->log_debug("Opening %s", source);
    input.open(source.c_str(), std::fstream::in | std::fstream::binary);
    if (input.is_open()) {
      input.seekg(offset, input.beg);
      while (input.good()) {
        bool invalidWrite = false;
        flowFile = std::static_pointer_cast<FlowFileRecord>(create());
        claim = std::make_shared<ResourceClaim>(process_context_->getContentRepository());
        uint64_t startTime = getTimeMillis();
        input.getline(buf, size, inputDelimiter);

        std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->write(claim);
        if (nullptr == stream) {
          logger_->log_debug("Stream is null");
          rollback();
          return;
        }

        if (input) {
          if (stream->write(reinterpret_cast<uint8_t*>(buf), size) < 0) {
            invalidWrite = true;
            break;
          }
        } else {
          if (stream->write(reinterpret_cast<uint8_t*>(buf), input.gcount()) < 0) {
            invalidWrite = true;
            break;
          }
        }

        if (!invalidWrite) {
          flowFile->setSize(stream->getSize());
          flowFile->setOffset(0);
          if (flowFile->getResourceClaim() != nullptr) {
            // Remove the old claim
            flowFile->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
            flowFile->clearResourceClaim();
          }
          flowFile->setResourceClaim(claim);
          claim->increaseFlowFileRecordOwnedCount();

          logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flowFile->getOffset(),
                             flowFile->getSize(),
                             flowFile->getResourceClaim()->getContentFullPath().c_str(),
                             flowFile->getUUIDStr().c_str());

          stream->closeStream();
          std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flowFile->getUUIDStr();
          uint64_t endTime = getTimeMillis();
          provenance_report_->modifyContent(flowFile, details, endTime - startTime);
          flows.push_back(flowFile);
        } else {
          logger_->log_debug("Error while writing");
          stream->closeStream();
          throw Exception(FILE_OPERATION_EXCEPTION, "File Export Error creating Flowfile");
        }
      }
      input.close();
      if (!keepSource)
        std::remove(source.c_str());
    } else {
      input.close();
      throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
    }

    delete[] buf;
  } catch (std::exception &exception) {
    if (flowFile && flowFile->getResourceClaim() == claim) {
      flowFile->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flowFile->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception %s", exception.what());
    delete[] buf;
    throw;
  } catch (...) {
    if (flowFile && flowFile->getResourceClaim() == claim) {
      flowFile->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flowFile->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception during process session write");
    delete[] buf;
    throw;
  }
}

void ProcessSession::import(std::string source, std::shared_ptr<core::FlowFile> &&flow, bool keepSource, uint64_t offset) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(process_context_->getContentRepository());
  char *buf = NULL;
  int size = 4096;
  buf = new char[size];

  try {
    //  std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    std::ifstream input;
    input.open(source.c_str(), std::fstream::in | std::fstream::binary);
    claim->increaseFlowFileRecordOwnedCount();
    std::shared_ptr<io::BaseStream> stream = process_context_->getContentRepository()->write(claim);
    if (nullptr == stream) {
      rollback();
      return;
    }
    if (input.is_open()) {
      // Open the source file and stream to the flow file
      input.seekg(offset);
      int sizeWritten = 0;
      bool invalidWrite = false;
      while (input.good()) {
        input.read(buf, size);
        if (input) {
          if (stream->write(reinterpret_cast<uint8_t*>(buf), size) < 0) {
            invalidWrite = true;
            break;
          }
        } else {
          if (stream->write(reinterpret_cast<uint8_t*>(buf), input.gcount()) < 0) {
            invalidWrite = true;
            break;
          }
        }
      }
      if (!invalidWrite) {
        flow->setSize(stream->getSize());
        flow->setOffset(0);
        if (flow->getResourceClaim() != nullptr) {
          // Remove the old claim
          flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);

        logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath().c_str(),
                           flow->getUUIDStr().c_str());

        stream->closeStream();
        input.close();
        if (!keepSource)
          std::remove(source.c_str());
        std::stringstream details;
        details << process_context_->getProcessorNode().getName() << " modify flow record content " << flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details.str(), endTime - startTime);
      } else {
        stream->closeStream();
        input.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
    }

    delete[] buf;
  } catch (std::exception &exception) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception %s", exception.what());
    delete[] buf;
    throw;
  } catch (...) {
    if (flow && flow->getResourceClaim() == claim) {
      flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
      flow->clearResourceClaim();
    }
    logger_->log_debug("Caught Exception during process session write");
    delete[] buf;
    throw;
  }
}

void ProcessSession::commit() {
  try {
    // First we clone the flow record based on the transfered relationship for updated flow record
    for (auto && it : _updatedFlowFiles) {
      std::shared_ptr<core::FlowFile> record = it.second;
      if (record->isDeleted())
        continue;
      std::map<std::string, Relationship>::iterator itRelationship = this->_transferRelationship.find(record->getUUIDStr());
      if (itRelationship != _transferRelationship.end()) {
        Relationship relationship = itRelationship->second;
        // Find the relationship, we need to find the connections for that relationship
        std::set<std::shared_ptr<Connectable>> connections = process_context_->getProcessorNode().getOutGoingConnections(relationship.getName());
        if (connections.empty()) {
          // No connection
          if (!process_context_->getProcessorNode().isAutoTerminated(relationship)) {
            // Not autoterminate, we should have the connect
            std::string message = "Connect empty for non auto terminated relationship" + relationship.getName();
            throw Exception(PROCESS_SESSION_EXCEPTION, message.c_str());
          } else {
            // Autoterminated
            remove(record);
          }
        } else {
          // We connections, clone the flow and assign the connection accordingly
          for (std::set<std::shared_ptr<Connectable>>::iterator itConnection = connections.begin(); itConnection != connections.end(); ++itConnection) {
            std::shared_ptr<Connectable> connection = *itConnection;
            if (itConnection == connections.begin()) {
              // First connection which the flow need be routed to
              record->setConnection(connection);
            } else {
              // Clone the flow file and route to the connection
              std::shared_ptr<core::FlowFile> cloneRecord;
              cloneRecord = this->cloneDuringTransfer(record);
              if (cloneRecord)
                cloneRecord->setConnection(connection);
              else
                throw Exception(PROCESS_SESSION_EXCEPTION, "Can not clone the flow for transfer");
            }
          }
        }
      } else {
        // Can not find relationship for the flow
        throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the flow");
      }
    }

    // Do the same thing for added flow file
    for (const auto it : _addedFlowFiles) {
      std::shared_ptr<core::FlowFile> record = it.second;
      if (record->isDeleted())
        continue;
      std::map<std::string, Relationship>::iterator itRelationship = this->_transferRelationship.find(record->getUUIDStr());
      if (itRelationship != _transferRelationship.end()) {
        Relationship relationship = itRelationship->second;
        // Find the relationship, we need to find the connections for that relationship
        std::set<std::shared_ptr<Connectable>> connections = process_context_->getProcessorNode().getOutGoingConnections(relationship.getName());
        if (connections.empty()) {
          // No connection
          if (!process_context_->getProcessorNode().isAutoTerminated(relationship)) {
            // Not autoterminate, we should have the connect
            std::string message = "Connect empty for non auto terminated relationship " + relationship.getName();
            throw Exception(PROCESS_SESSION_EXCEPTION, message.c_str());
          } else {
            logger_->log_debug("added flow file is auto terminated");
            // Autoterminated
            remove(record);
          }
        } else {
          // We connections, clone the flow and assign the connection accordingly
          for (std::set<std::shared_ptr<Connectable>>::iterator itConnection = connections.begin(); itConnection != connections.end(); ++itConnection) {
            std::shared_ptr<Connectable> connection(*itConnection);
            if (itConnection == connections.begin()) {
              // First connection which the flow need be routed to
              record->setConnection(connection);
            } else {
              // Clone the flow file and route to the connection
              std::shared_ptr<core::FlowFile> cloneRecord;
              cloneRecord = this->cloneDuringTransfer(record);
              if (cloneRecord)
                cloneRecord->setConnection(connection);
              else
                throw Exception(PROCESS_SESSION_EXCEPTION, "Can not clone the flow for transfer");
            }
          }
        }
      } else {
        // Can not find relationship for the flow
        throw Exception(PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the flow");
      }
    }

    std::shared_ptr<Connection> connection = nullptr;
    // Complete process the added and update flow files for the session, send the flow file to its queue
    for (const auto &it : _updatedFlowFiles) {
      std::shared_ptr<core::FlowFile> record = it.second;
      if (record->isDeleted()) {
        continue;
      }

      connection = std::static_pointer_cast<Connection>(record->getConnection());
      if ((connection) != nullptr)
        connection->put(record);
    }
    for (const auto &it : _addedFlowFiles) {
      std::shared_ptr<core::FlowFile> record = it.second;
      if (record->isDeleted()) {
        continue;
      }
      connection = std::static_pointer_cast<Connection>(record->getConnection());
      if ((connection) != nullptr)
        connection->put(record);
    }
    // Process the clone flow files
    for (const auto &it : _clonedFlowFiles) {
      std::shared_ptr<core::FlowFile> record = it.second;
      if (record->isDeleted()) {
        continue;
      }
      connection = std::static_pointer_cast<Connection>(record->getConnection());
      if ((connection) != nullptr)
        connection->put(record);
    }

    // All done
    _updatedFlowFiles.clear();
    _addedFlowFiles.clear();
    _clonedFlowFiles.clear();
    _deletedFlowFiles.clear();
    _originalFlowFiles.clear();
    // persistent the provenance report
    this->provenance_report_->commit();
    logger_->log_trace("ProcessSession committed for %s", process_context_->getProcessorNode().getName().c_str());
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session commit");
    throw;
  }
}

void ProcessSession::rollback() {
  try {
    std::shared_ptr<Connection> connection = nullptr;
    // Requeue the snapshot of the flowfile back
    for (const auto &it : _originalFlowFiles) {
      std::shared_ptr<core::FlowFile> record = it.second;
      connection = std::static_pointer_cast<Connection>(record->getOriginalConnection());
      if ((connection) != nullptr) {
        std::shared_ptr<FlowFileRecord> flowf = std::static_pointer_cast<FlowFileRecord>(record);
        flowf->setSnapShot(false);
        connection->put(record);
      }
    }
    _originalFlowFiles.clear();

    _clonedFlowFiles.clear();
    _addedFlowFiles.clear();
    _updatedFlowFiles.clear();
    _deletedFlowFiles.clear();
    logger_->log_debug("ProcessSession rollback for %s", process_context_->getProcessorNode().getName().c_str());
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception during process session roll back");
    throw;
  }
}

std::shared_ptr<core::FlowFile> ProcessSession::get() {
  std::shared_ptr<Connectable> first = process_context_->getProcessorNode().getNextIncomingConnection();

  if (first == NULL) {
    logger_->log_debug("Get is null for %s", process_context_->getProcessorNode().getName());
    return NULL;
  }

  std::shared_ptr<Connection> current = std::static_pointer_cast<Connection>(first);

  do {
    std::set<std::shared_ptr<core::FlowFile> > expired;
    std::shared_ptr<core::FlowFile> ret = current->poll(expired);
    if (expired.size() > 0) {
      // Remove expired flow record
      for (std::set<std::shared_ptr<core::FlowFile> >::iterator it = expired.begin(); it != expired.end(); ++it) {
        std::shared_ptr<core::FlowFile> record = *it;
        std::stringstream details;
        details << process_context_->getProcessorNode().getName() << " expire flow record " << record->getUUIDStr();
        provenance_report_->expire(record, details.str());
      }
    }
    if (ret) {
      // add the flow record to the current process session update map
      ret->setDeleted(false);
      _updatedFlowFiles[ret->getUUIDStr()] = ret;
      std::map<std::string, std::string> empty;
      std::shared_ptr<core::FlowFile> snapshot = std::make_shared<FlowFileRecord>(process_context_->getFlowFileRepository(), process_context_->getContentRepository(), empty);
      logger_->log_debug("Create Snapshot FlowFile with UUID %s", snapshot->getUUIDStr().c_str());
      snapshot = ret;
      // save a snapshot
      _originalFlowFiles[snapshot->getUUIDStr()] = snapshot;
      return ret;
    }
    current = std::static_pointer_cast<Connection>(process_context_->getProcessorNode().getNextIncomingConnection());
  } while (current != NULL && current != first);

  return NULL;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
