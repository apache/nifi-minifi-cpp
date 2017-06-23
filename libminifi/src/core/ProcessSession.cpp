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
  std::shared_ptr<core::FlowFile> record = std::make_shared<FlowFileRecord>(process_context_->getProvenanceRepository(), empty);

  _addedFlowFiles[record->getUUIDStr()] = record;
  logger_->log_debug("Create FlowFile with UUID %s", record->getUUIDStr().c_str());
  std::string details = process_context_->getProcessorNode().getName() + " creates flow record " + record->getUUIDStr();
  provenance_report_->create(record, details);

  return record;
}

std::shared_ptr<core::FlowFile> ProcessSession::create(std::shared_ptr<core::FlowFile> &&parent) {
  std::map<std::string, std::string> empty;
  std::shared_ptr<core::FlowFile> record = std::make_shared<FlowFileRecord>(process_context_->getProvenanceRepository(), empty);

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
  std::shared_ptr<core::FlowFile> record = std::make_shared<FlowFileRecord>(process_context_->getProvenanceRepository(), empty);

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
  std::string details = process_context_->getProcessorNode().getName() + " modify flow record " + flow->getUUIDStr() + " attribute " + key + ":" + value;
  provenance_report_->modifyAttributes(flow, details);
}

void ProcessSession::removeAttribute(std::shared_ptr<core::FlowFile> &flow, std::string key) {
  flow->removeAttribute(key);
  std::string details = process_context_->getProcessorNode().getName() + " remove flow record " + flow->getUUIDStr() + " attribute " + key;
  provenance_report_->modifyAttributes(flow, details);
}

void ProcessSession::putAttribute(std::shared_ptr<core::FlowFile> &&flow, std::string key, std::string value) {
  flow->setAttribute(key, value);
  std::string details = process_context_->getProcessorNode().getName() + " modify flow record " + flow->getUUIDStr() + " attribute " + key + ":" + value;
  provenance_report_->modifyAttributes(flow, details);
}

void ProcessSession::removeAttribute(std::shared_ptr<core::FlowFile> &&flow, std::string key) {
  flow->removeAttribute(key);
  std::string details = process_context_->getProcessorNode().getName() + " remove flow record " + flow->getUUIDStr() + " attribute " + key;
  provenance_report_->modifyAttributes(flow, details);
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
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>(
  DEFAULT_CONTENT_DIRECTORY);

  try {
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if (fs.is_open()) {
      // Call the callback to write the content
      callback->process(&fs);
      if (fs.good() && fs.tellp() >= 0) {
        flow->setSize(fs.tellp());
        flow->setOffset(0);
        std::shared_ptr<ResourceClaim> flow_claim = flow->getResourceClaim();
        if (flow_claim != nullptr) {
          // Remove the old claim
          flow_claim->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);
        claim->increaseFlowFileRecordOwnedCount();
        /*
         logger_->log_debug("Write offset %d length %d into content %s for FlowFile UUID %s",
         flow->_offset, flow->_size, flow->_claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str()); */
        fs.close();
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Write Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
    }
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
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>();
  try {
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if (fs.is_open()) {
      // Call the callback to write the content
      callback->process(&fs);
      if (fs.good() && fs.tellp() >= 0) {
        flow->setSize(fs.tellp());
        flow->setOffset(0);
        std::shared_ptr<ResourceClaim> flow_claim = flow->getResourceClaim();
        if (flow_claim != nullptr) {
          // Remove the old claim
          flow_claim->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);
        claim->increaseFlowFileRecordOwnedCount();
        /*
         logger_->log_debug("Write offset %d length %d into content %s for FlowFile UUID %s",
         flow->_offset, flow->_size, flow->_claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str()); */
        fs.close();
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Write Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
    }
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
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::app);
    if (fs.is_open()) {
      // Call the callback to write the content
      std::streampos oldPos = fs.tellp();
      callback->process(&fs);
      if (fs.good() && fs.tellp() >= 0) {
        uint64_t appendSize = fs.tellp() - oldPos;
        flow->setSize(flow->getSize() + appendSize);
        /*
         logger_->log_debug("Append offset %d extra length %d to new size %d into content %s for FlowFile UUID %s",
         flow->_offset, appendSize, flow->_size, claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str()); */
        fs.close();
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Write Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
    }
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
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::app);
    if (fs.is_open()) {
      // Call the callback to write the content
      std::streampos oldPos = fs.tellp();
      callback->process(&fs);
      if (fs.good() && fs.tellp() >= 0) {
        uint64_t appendSize = fs.tellp() - oldPos;
        flow->setSize(flow->getSize() + appendSize);
        /*
         logger_->log_debug("Append offset %d extra length %d to new size %d into content %s for FlowFile UUID %s",
         flow->_offset, appendSize, flow->_size, claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str()); */
        fs.close();
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Write Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
    }
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
    std::ifstream fs;
    fs.open(claim->getContentFullPath().c_str(), std::fstream::in | std::fstream::binary);
    if (fs.is_open()) {
      fs.seekg(flow->getOffset(), fs.beg);

      if (fs.good()) {
        callback->process(&fs);
        /*
         logger_->log_debug("Read offset %d size %d content %s for FlowFile UUID %s",
         flow->_offset, flow->_size, claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str()); */
        fs.close();
      } else {
        fs.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Read Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
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
    std::ifstream fs;
    fs.open(claim->getContentFullPath().c_str(), std::fstream::in | std::fstream::binary);
    if (fs.is_open()) {
      fs.seekg(flow->getOffset(), fs.beg);

      if (fs.good()) {
        callback->process(&fs);
        /*
         logger_->log_debug("Read offset %d size %d content %s for FlowFile UUID %s",
         flow->_offset, flow->_size, claim->getContentFullPath().c_str(), flow->getUUIDStr().c_str()); */
        fs.close();
      } else {
        fs.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Read Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Open Error");
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
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>();

  int max_read = getpagesize();
  std::vector<uint8_t> charBuffer;
  charBuffer.resize(max_read);

  try {
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);

    if (fs.is_open()) {
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

        fs.write((const char*) charBuffer.data(), read_size);
        position += read_size;
      }
      // Open the source file and stream to the flow file

      if (fs.good() && fs.tellp() >= 0) {
        flow->setSize(fs.tellp());
        flow->setOffset(0);
        if (flow->getResourceClaim() != nullptr) {
          // Remove the old claim
          flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);
        claim->increaseFlowFileRecordOwnedCount();

        logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath().c_str(),
                           flow->getUUIDStr().c_str());

        fs.close();
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
        throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
      }
    } else {
      throw Exception(FILE_OPERATION_EXCEPTION, "File Import Error");
    }
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

void ProcessSession::import(std::string source, std::shared_ptr<core::FlowFile> &flow,
bool keepSource,
                            uint64_t offset) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>();
  char *buf = NULL;
  int size = 4096;
  buf = new char[size];

  try {
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
    std::ifstream input;
    input.open(source.c_str(), std::fstream::in | std::fstream::binary);

    if (fs.is_open() && input.is_open()) {
      // Open the source file and stream to the flow file
      input.seekg(offset, fs.beg);
      while (input.good()) {
        input.read(buf, size);
        if (input)
          fs.write(buf, size);
        else
          fs.write(buf, input.gcount());
      }

      if (fs.good() && fs.tellp() >= 0) {
        flow->setSize(fs.tellp());
        flow->setOffset(0);
        if (flow->getResourceClaim() != nullptr) {
          // Remove the old claim
          flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);
        claim->increaseFlowFileRecordOwnedCount();

        logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath().c_str(),
                           flow->getUUIDStr().c_str());

        fs.close();
        input.close();
        if (!keepSource)
          std::remove(source.c_str());
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
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

void ProcessSession::import(std::string source, std::shared_ptr<core::FlowFile> &&flow,
  bool keepSource, uint64_t offset, char inputDelimiter) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>();

  char *buf = NULL;
  int size = 4096;
  buf = new char[size];

  try {
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
    std::ifstream input;
    input.open(source.c_str(), std::fstream::in | std::fstream::binary);

    if (fs.is_open() && input.is_open()) {
      // Open the source file and stream to the flow file
      input.seekg(offset, fs.beg);
      while (input.good()) {
        input.getline(buf, size, inputDelimiter);
        if (input)
          fs.write(buf, size);
        else
          fs.write(buf, input.gcount());
      }

      if (fs.good() && fs.tellp() >= 0) {
        flow->setSize(fs.tellp());
        flow->setOffset(0);
        if (flow->getResourceClaim() != nullptr) {
          // Remove the old claim
          flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);
        claim->increaseFlowFileRecordOwnedCount();

        logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath().c_str(),
                           flow->getUUIDStr().c_str());

        fs.close();
        input.close();
        if (!keepSource)
          std::remove(source.c_str());
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
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

void ProcessSession::import(std::string source, std::shared_ptr<core::FlowFile> &&flow,
bool keepSource,
                            uint64_t offset) {
  std::shared_ptr<ResourceClaim> claim = std::make_shared<ResourceClaim>();

  char *buf = NULL;
  int size = 4096;
  buf = new char[size];

  try {
    std::ofstream fs;
    uint64_t startTime = getTimeMillis();
    fs.open(claim->getContentFullPath().c_str(), std::fstream::out | std::fstream::binary | std::fstream::trunc);
    std::ifstream input;
    input.open(source.c_str(), std::fstream::in | std::fstream::binary);

    if (fs.is_open() && input.is_open()) {
      // Open the source file and stream to the flow file
      input.seekg(offset, fs.beg);
      while (input.good()) {
        input.read(buf, size);
        if (input)
          fs.write(buf, size);
        else
          fs.write(buf, input.gcount());
      }

      if (fs.good() && fs.tellp() >= 0) {
        flow->setSize(fs.tellp());
        flow->setOffset(0);
        if (flow->getResourceClaim() != nullptr) {
          // Remove the old claim
          flow->getResourceClaim()->decreaseFlowFileRecordOwnedCount();
          flow->clearResourceClaim();
        }
        flow->setResourceClaim(claim);
        claim->increaseFlowFileRecordOwnedCount();

        logger_->log_debug("Import offset %d length %d into content %s for FlowFile UUID %s", flow->getOffset(), flow->getSize(), flow->getResourceClaim()->getContentFullPath().c_str(),
                           flow->getUUIDStr().c_str());

        fs.close();
        input.close();
        if (!keepSource)
          std::remove(source.c_str());
        std::string details = process_context_->getProcessorNode().getName() + " modify flow record content " + flow->getUUIDStr();
        uint64_t endTime = getTimeMillis();
        provenance_report_->modifyContent(flow, details, endTime - startTime);
      } else {
        fs.close();
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

    // Do the samething for added flow file
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
    logger_->log_trace("ProcessSession rollback for %s", process_context_->getProcessorNode().getName().c_str());
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

  if (first == NULL)
    return NULL;

  std::shared_ptr<Connection> current = std::static_pointer_cast<Connection>(first);

  do {
    std::set<std::shared_ptr<core::FlowFile> > expired;
    std::shared_ptr<core::FlowFile> ret = current->poll(expired);
    if (expired.size() > 0) {
      // Remove expired flow record
      for (std::set<std::shared_ptr<core::FlowFile> >::iterator it = expired.begin(); it != expired.end(); ++it) {
        std::shared_ptr<core::FlowFile> record = *it;
        std::string details = process_context_->getProcessorNode().getName() + " expire flow record " + record->getUUIDStr();
        provenance_report_->expire(record, details);
      }
    }
    if (ret) {
      // add the flow record to the current process session update map
      ret->setDeleted(false);
      _updatedFlowFiles[ret->getUUIDStr()] = ret;
      std::map<std::string, std::string> empty;
      std::shared_ptr<core::FlowFile> snapshot = std::make_shared<FlowFileRecord>(process_context_->getProvenanceRepository(), empty);
      logger_->log_debug("Create Snapshot FlowFile with UUID %s", snapshot->getUUIDStr().c_str());
      snapshot = ret;
//      snapshot->duplicate(ret);
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
