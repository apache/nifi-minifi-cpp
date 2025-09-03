/**
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

#include "io/InputStream.h"
#include "io/OutputStream.h"
#include "utils/minifi-c-utils.h"

namespace org::apache::nifi::minifi::core {

namespace {

class MinifiOutputStreamWrapper : public io::OutputStreamImpl {
 public:
  explicit MinifiOutputStreamWrapper(MinifiOutputStream impl): impl_(impl) {}

  size_t write(const uint8_t *value, size_t len) override {
    return MinifiOutputStreamWrite(impl_, reinterpret_cast<const char*>(value), len);
  }

  void close() override {gsl_FailFast();}
  void seek(size_t /*offset*/) override {gsl_FailFast();}
  [[nodiscard]] size_t tell() const override {gsl_FailFast();}
  int initialize() override {gsl_FailFast();}
  [[nodiscard]] std::span<const std::byte> getBuffer() const override {gsl_FailFast();}

 private:
  MinifiOutputStream impl_;
};

class MinifiInputStreamWrapper : public io::InputStreamImpl {
public:
  explicit MinifiInputStreamWrapper(MinifiInputStream impl): impl_(impl) {}

  size_t read(std::span<std::byte> out_buffer) override {
    return MinifiInputStreamRead(impl_, reinterpret_cast<char*>(out_buffer.data()), out_buffer.size());
  }

  [[nodiscard]] size_t size() const override {
    return MinifiInputStreamSize(impl_);
  }

  void close() override {gsl_FailFast();}
  void seek(size_t /*offset*/) override {gsl_FailFast();}
  [[nodiscard]] size_t tell() const override {gsl_FailFast();}
  int initialize() override {gsl_FailFast();}
  [[nodiscard]] std::span<const std::byte> getBuffer() const override {gsl_FailFast();}

private:
  MinifiInputStream impl_;
};

}

std::shared_ptr<FlowFile> ProcessSession::get() {
  return std::make_shared<FlowFile>(MinifiProcessSessionGet(impl_));
}

std::shared_ptr<FlowFile> ProcessSession::create(const FlowFile* parent) {
  return std::make_shared<FlowFile>(MinifiProcessSessionCreate(impl_, parent ? parent->getImpl() : MINIFI_NULL));
}

void ProcessSession::transfer(const std::shared_ptr<FlowFile>& ff, const Relationship& relationship) {
  const auto rel_name = relationship.getName();
  MinifiProcessSessionTransfer(impl_, ff->getImpl(), utils::toStringView(rel_name));
}

void ProcessSession::writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const char> buffer) {
  writeBuffer(flow_file, as_bytes(buffer));
}

void ProcessSession::writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const std::byte> buffer) {
  write(*flow_file, [buffer](const std::shared_ptr<io::OutputStream>& output_stream) {
    const auto write_status = output_stream->write(buffer);
    return io::isError(write_status) ? -1 : gsl::narrow<int64_t>(write_status);
  });
}

std::vector<std::byte> ProcessSession::readBuffer(core::FlowFile& flow_file) {
  std::vector<std::byte> result;
  read(flow_file, [&result](const std::shared_ptr<io::InputStream>& input_stream) {
    result.resize(input_stream->size());
    const auto read_status = input_stream->read(result);
    return io::isError(read_status) ? -1 : gsl::narrow<int64_t>(read_status);
  });
  return result;
}

void ProcessSession::write(core::FlowFile& flow_file, const io::OutputStreamCallback& callback) {
  MinifiProcessSessionWrite(impl_, flow_file.getImpl(), [] (void* data, MinifiOutputStream output) {
    return (*reinterpret_cast<const io::OutputStreamCallback*>(data))(std::make_shared<MinifiOutputStreamWrapper>(output));
  }, (void*)&callback);
}

void ProcessSession::read(core::FlowFile& flow_file, const io::InputStreamCallback& callback) {
  MinifiProcessSessionRead(impl_, flow_file.getImpl(), [] (void* data, MinifiInputStream input) {
    return (*reinterpret_cast<const io::InputStreamCallback*>(data))(std::make_shared<MinifiInputStreamWrapper>(input));
  }, (void*)&callback);
}

}  // namespace org::apache::nifi::minifi::cpp::core

