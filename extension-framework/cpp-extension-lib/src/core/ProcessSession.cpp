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

#include "api/core/ProcessSession.h"

#include "io/InputStream.h"
#include "io/OutputStream.h"
#include "api/utils/minifi-c-utils.h"
#include "api/core/FlowFile.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::api::core {

namespace {

class MinifiOutputStreamWrapper : public io::OutputStreamImpl {
 public:
  explicit MinifiOutputStreamWrapper(MinifiOutputStream* impl): impl_(impl) {}

  size_t write(const uint8_t *value, size_t len) override {
    return MinifiOutputStreamWrite(impl_, reinterpret_cast<const char*>(value), len);
  }

  void close() override {gsl_FailFast();}
  void seek(size_t /*offset*/) override {gsl_FailFast();}
  [[nodiscard]] size_t tell() const override {gsl_FailFast();}
  int initialize() override {gsl_FailFast();}
  [[nodiscard]] std::span<const std::byte> getBuffer() const override {gsl_FailFast();}

 private:
  MinifiOutputStream* impl_;
};

class MinifiInputStreamWrapper : public io::InputStreamImpl {
 public:
  explicit MinifiInputStreamWrapper(MinifiInputStream* impl): impl_(impl) {}

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
  MinifiInputStream* impl_;
};

}  // namespace

FlowFile ProcessSession::get() {
  return FlowFile{MinifiProcessSessionGet(impl_)};
}

FlowFile ProcessSession::create(const FlowFile* parent) {
  return FlowFile{MinifiProcessSessionCreate(impl_, parent ? parent->get() : MINIFI_NULL)};
}

void ProcessSession::transfer(FlowFile ff, const minifi::core::Relationship& relationship) {
  const auto rel_name = relationship.getName();
  if (MINIFI_STATUS_SUCCESS != MinifiProcessSessionTransfer(impl_, ff.release(), utils::toStringView(rel_name))) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to transfer flowfile");
  }
}

void ProcessSession::remove(FlowFile ff) {
  if (MINIFI_STATUS_SUCCESS != MinifiProcessSessionRemove(impl_, ff.release())) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to remove flowfile");
  }
}

void ProcessSession::write(FlowFile& flow_file, const io::OutputStreamCallback& callback) {
  const auto status = MinifiProcessSessionWrite(impl_, flow_file.get(), [] (void* data, MinifiOutputStream* output) {
    return (*static_cast<const io::OutputStreamCallback*>(data))(std::make_shared<MinifiOutputStreamWrapper>(output));
  }, const_cast<io::OutputStreamCallback*>(&callback));
  if (status != MINIFI_STATUS_SUCCESS) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
  }
}

void ProcessSession::read(FlowFile& flow_file, const io::InputStreamCallback& callback) {
  const auto status = MinifiProcessSessionRead(impl_, flow_file.get(), [] (void* data, MinifiInputStream* input) {
    return (*static_cast<const io::InputStreamCallback*>(data))(std::make_shared<MinifiInputStreamWrapper>(input));
  }, const_cast<io::InputStreamCallback*>(&callback));
  if (status != MINIFI_STATUS_SUCCESS) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
  }
}

void ProcessSession::setAttribute(FlowFile& ff, const std::string_view key, std::string value) {  // NOLINT(performance-unnecessary-value-param)
  const MinifiStringView value_ref = utils::toStringView(value);
  if (MINIFI_STATUS_SUCCESS != MinifiFlowFileSetAttribute(impl_, ff.get(), utils::toStringView(key), &value_ref)) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to set attribute");
  }
}

void ProcessSession::removeAttribute(FlowFile& ff, const std::string_view key) {
  if (MINIFI_STATUS_SUCCESS != MinifiFlowFileSetAttribute(impl_, ff.get(), utils::toStringView(key), nullptr)) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to remove attribute");
  }
}

std::optional<std::string> ProcessSession::getAttribute(FlowFile& ff, std::string_view key) {
  std::optional<std::string> result;
  MinifiFlowFileGetAttribute(impl_, ff.get(), utils::toStringView(key), [] (void* user_ctx, MinifiStringView value) {
    *static_cast<std::optional<std::string>*>(user_ctx) = std::string{value.data, value.length};
  }, &result);
  return result;
}

std::map<std::string, std::string> ProcessSession::getAttributes(FlowFile& ff) {
  std::map<std::string, std::string> result;
  MinifiFlowFileGetAttributes(impl_, ff.get(), [] (void* user_ctx, MinifiStringView value, MinifiStringView key) {
    static_cast<std::map<std::string, std::string>*>(user_ctx)->insert({std::string{value.data, value.length}, std::string{key.data, key.length}});
  }, &result);
  return result;
}

void ProcessSession::writeBuffer(FlowFile& flow_file, std::span<const char> buffer) {
  writeBuffer(flow_file, as_bytes(buffer));
}

void ProcessSession::writeBuffer(FlowFile& flow_file, std::span<const std::byte> buffer) {
  write(flow_file, [buffer](const std::shared_ptr<io::OutputStream>& output_stream) {
    const auto write_status = output_stream->write(buffer);
    return io::isError(write_status) ? -1 : gsl::narrow<int64_t>(write_status);
  });
}

std::vector<std::byte> ProcessSession::readBuffer(FlowFile& flow_file) {
  std::vector<std::byte> result;
  read(flow_file, [&result](const std::shared_ptr<io::InputStream>& input_stream) {
    result.resize(input_stream->size());
    const auto read_status = input_stream->read(result);
    return io::isError(read_status) ? -1 : gsl::narrow<int64_t>(read_status);
  });
  return result;
}

}  // namespace org::apache::nifi::minifi::api::core
