/**
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
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "ProcessContext.h"
#include "core/logging/LoggerFactory.h"
#include "core/Deprecated.h"
#include "FlowFile.h"
#include "WeakReference.h"
#include "minifi-cpp/provenance/Provenance.h"
#include "utils/gsl.h"
#include "ProcessorMetrics.h"
#include "minifi-cpp/io/StreamCallback.h"

namespace org::apache::nifi::minifi::core {
namespace detail {
struct ReadBufferResult {
  int64_t status;
  std::vector<std::byte> buffer;
};

std::string to_string(const ReadBufferResult& read_buffer_result);
}  // namespace detail

// ProcessSession Class
class ProcessSession : public ReferenceContainer {
 public:
  ~ProcessSession() override = default;

  virtual void commit() = 0;
  virtual void rollback() = 0;

  virtual nonstd::expected<void, std::exception_ptr> rollbackNoThrow() noexcept = 0;
  virtual std::shared_ptr<provenance::ProvenanceReporter> getProvenanceReporter() = 0;
  virtual void flushContent() = 0;
  virtual std::shared_ptr<core::FlowFile> get() = 0;
  virtual std::shared_ptr<core::FlowFile> create(const core::FlowFile* const parent = nullptr) = 0;
  virtual void add(const std::shared_ptr<core::FlowFile> &record) = 0;
  virtual std::shared_ptr<core::FlowFile> clone(const core::FlowFile& parent) = 0;
  virtual std::shared_ptr<core::FlowFile> clone(const core::FlowFile& parent, int64_t offset, int64_t size) = 0;
  virtual void transfer(const std::shared_ptr<core::FlowFile>& flow, const Relationship& relationship) = 0;
  virtual void transferToCustomRelationship(const std::shared_ptr<core::FlowFile>& flow, const std::string& relationship_name) = 0;

  virtual void putAttribute(core::FlowFile& flow, std::string_view key, const std::string& value) = 0;
  virtual void removeAttribute(core::FlowFile& flow, std::string_view key) = 0;

  virtual void remove(const std::shared_ptr<core::FlowFile> &flow) = 0;
  // Access the contents of the flow file as an input stream; returns null if the flow file has no content claim
  virtual std::shared_ptr<io::InputStream> getFlowFileContentStream(const core::FlowFile& flow_file) = 0;
  // Execute the given read callback against the content
  virtual int64_t read(const std::shared_ptr<core::FlowFile>& flow_file, const io::InputStreamCallback& callback) = 0;

  virtual int64_t read(const core::FlowFile& flow_file, const io::InputStreamCallback& callback) = 0;
  // Read content into buffer
  virtual detail::ReadBufferResult readBuffer(const std::shared_ptr<core::FlowFile>& flow) = 0;
  // Execute the given write callback against the content
  virtual void write(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback) = 0;

  virtual void write(core::FlowFile& flow, const io::OutputStreamCallback& callback) = 0;
  // Read and write the flow file at the same time (eg. for processing it line by line)
  virtual int64_t readWrite(const std::shared_ptr<core::FlowFile> &flow, const io::InputOutputStreamCallback& callback) = 0;
  // Replace content with buffer
  virtual void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const char> buffer) = 0;
  virtual void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const std::byte> buffer) = 0;
  // Execute the given write/append callback against the content
  virtual void append(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback) = 0;
  // Append buffer to content
  virtual void appendBuffer(const std::shared_ptr<core::FlowFile>& flow, std::span<const char> buffer) = 0;
  virtual void appendBuffer(const std::shared_ptr<core::FlowFile>& flow, std::span<const std::byte> buffer) = 0;
  // Penalize the flow
  virtual void penalize(const std::shared_ptr<core::FlowFile> &flow) = 0;

  virtual bool outgoingConnectionsFull(const std::string& relationship) = 0;

  /**
   * Imports a file from the data stream
   * @param stream incoming data stream that contains the data to store into a file
   * @param flow flow file
   */
  virtual void importFrom(io::InputStream &stream, const std::shared_ptr<core::FlowFile> &flow) = 0;
  virtual void importFrom(io::InputStream&& stream, const std::shared_ptr<core::FlowFile> &flow) = 0;

  // import from the data source.
  virtual void import(const std::string& source, const std::shared_ptr<core::FlowFile> &flow, bool keepSource = true, uint64_t offset = 0) = 0;
  DEPRECATED(/*deprecated in*/ 0.7.0, /*will remove in */ 2.0) virtual void import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, bool keepSource, uint64_t offset, char inputDelimiter) = 0; // NOLINT
  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) virtual void import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, uint64_t offset, char inputDelimiter) = 0;

  /**
   * Exports the data stream to a file
   * @param string file to export stream to
   * @param flow flow file
   * @param bool whether or not to keep the content in the flow file
   */
  virtual bool exportContent(const std::string &destination, const std::shared_ptr<core::FlowFile> &flow, bool keepContent) = 0;

  virtual bool exportContent(const std::string &destination, const std::string &tmpFileName, const std::shared_ptr<core::FlowFile> &flow, bool keepContent) = 0;

  // Stash the content to a key
  virtual void stash(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) = 0;
  // Restore content previously stashed to a key
  virtual void restore(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) = 0;

  virtual bool existsFlowFileInRelationship(const Relationship &relationship) = 0;

  virtual void setMetrics(const std::shared_ptr<ProcessorMetrics>& metrics) = 0;

  virtual bool hasBeenTransferred(const core::FlowFile &flow) const = 0;
};

}  // namespace org::apache::nifi::minifi::core
