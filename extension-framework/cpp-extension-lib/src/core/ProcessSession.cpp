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

#include "api/core/FlowFile.h"
#include "api/utils/minifi-c-utils.h"
#include "io/InputStream.h"
#include "io/OutputStream.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::api::core {

namespace {

class MinifiOutputStreamWrapper : public io::OutputStreamImpl {
 public:
  explicit MinifiOutputStreamWrapper(minifi_output_stream* impl): impl_(impl) {}

  size_t write(const uint8_t *value, size_t len) override {
    return minifi_output_stream_write(impl_, reinterpret_cast<const char*>(value), len);
  }

  void close() override {gsl_FailFast();}
  void seek(size_t /*offset*/) override {gsl_FailFast();}
  [[nodiscard]] size_t tell() const override {gsl_FailFast();}
  int initialize() override {gsl_FailFast();}
  [[nodiscard]] std::span<const std::byte> getBuffer() const override {gsl_FailFast();}

 private:
  minifi_output_stream* impl_;
};

class MinifiInputStreamWrapper : public io::InputStreamImpl {
 public:
  explicit MinifiInputStreamWrapper(minifi_input_stream* impl): impl_(impl) {}

  size_t read(std::span<std::byte> out_buffer) override {
    return minifi_input_stream_read(impl_, reinterpret_cast<char*>(out_buffer.data()), out_buffer.size());
  }

  [[nodiscard]] size_t size() const override {
    return minifi_input_stream_size(impl_);
  }

  void close() override {gsl_FailFast();}
  void seek(size_t /*offset*/) override {gsl_FailFast();}
  [[nodiscard]] size_t tell() const override {gsl_FailFast();}
  int initialize() override {gsl_FailFast();}
  [[nodiscard]] std::span<const std::byte> getBuffer() const override {gsl_FailFast();}

 private:
  minifi_input_stream* impl_;
};

}  // namespace

FlowFile CffiProcessSession::get() {
  return FlowFile{minifi_process_session_get(impl_)};
}

FlowFile CffiProcessSession::create(const FlowFile* parent) {
  return FlowFile{minifi_process_session_create(impl_, parent ? parent->get() : nullptr)};
}

void CffiProcessSession::penalize(FlowFile& ff) {
  if (MINIFI_STATUS_SUCCESS != minifi_process_session_penalize(impl_, ff.get())) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to penalize flowfile");
  }
}

void CffiProcessSession::transfer(FlowFile ff, const minifi::core::Relationship& relationship) {
  const auto rel_name = relationship.getName();
  if (MINIFI_STATUS_SUCCESS != minifi_process_session_transfer(impl_, ff.release(), utils::minifiStringView(rel_name))) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to transfer flowfile");
  }
}

void CffiProcessSession::remove(FlowFile ff) {
  if (MINIFI_STATUS_SUCCESS != minifi_process_session_remove(impl_, ff.release())) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to remove flowfile");
  }
}

void CffiProcessSession::write(FlowFile& flow_file, const io::OutputStreamCallback& callback) {
  const auto status = minifi_process_session_write(
      impl_,
      flow_file.get(),
      [](void* data, minifi_output_stream* output) -> int64_t {
        const auto result =
            (*static_cast<const io::OutputStreamCallback*>(data))(std::make_shared<MinifiOutputStreamWrapper>(output));
        return result.toI64();
      },
      const_cast<io::OutputStreamCallback*>(&callback));
  if (status != MINIFI_STATUS_SUCCESS) { throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to process flowfile content"); }
}

void CffiProcessSession::read(FlowFile& flow_file, const io::InputStreamCallback& callback) {
  const auto status = minifi_process_session_read(
      impl_,
      flow_file.get(),
      [](void* data, minifi_input_stream* input) -> int64_t {
        const auto result =
            (*static_cast<const io::InputStreamCallback*>(data))(std::make_shared<MinifiInputStreamWrapper>(input));
        return result.toI64();
      },
      const_cast<io::InputStreamCallback*>(&callback));
  if (status != MINIFI_STATUS_SUCCESS) { throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to process flowfile content"); }
}

void CffiProcessSession::setAttribute(FlowFile& ff, const std::string_view key, std::string value) {  // NOLINT(performance-unnecessary-value-param)
  const minifi_string_view value_ref = utils::minifiStringView(value);
  if (MINIFI_STATUS_SUCCESS != minifi_process_session_set_flow_file_attribute(impl_, ff.get(), utils::minifiStringView(key), &value_ref)) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to set attribute");
  }
}

void CffiProcessSession::removeAttribute(FlowFile& ff, const std::string_view key) {
  if (MINIFI_STATUS_SUCCESS != minifi_process_session_set_flow_file_attribute(impl_, ff.get(), utils::minifiStringView(key), nullptr)) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to remove attribute");
  }
}

std::optional<std::string> CffiProcessSession::getAttribute(FlowFile& ff, std::string_view key) {
  std::optional<std::string> result;
  minifi_process_session_get_flow_file_attribute(impl_, ff.get(), utils::minifiStringView(key), [] (void* user_ctx, minifi_string_view value) {
    *static_cast<std::optional<std::string>*>(user_ctx) = std::string{value.data, value.length};
  }, &result);
  return result;
}

std::map<std::string, std::string> CffiProcessSession::getAttributes(const FlowFile& ff) const {
  std::map<std::string, std::string> result;
  minifi_process_session_get_flow_file_attributes(impl_, ff.get(), [] (void* user_ctx, const minifi_string_view key, const minifi_string_view value) {
    static_cast<std::map<std::string, std::string>*>(user_ctx)->insert({std::string{key.data, key.length}, std::string{value.data, value.length}});
  }, &result);
  return result;
}

std::string CffiProcessSession::getFlowFileId(const FlowFile& ff) const {
  std::string result;
  minifi_process_session_get_flow_file_id(impl_, ff.get(), [](void* user_ctx, const minifi_string_view value) {
      *static_cast<std::string*>(user_ctx) = std::string{value.data, value.length};
  }, &result);
  return result;
}

uint64_t CffiProcessSession::getFlowFileSize(const FlowFile& ff) const {
  return minifi_process_session_get_flow_file_size(impl_, ff.get());
}

void ProcessSession::writeBuffer(FlowFile& flow_file, std::span<const char> buffer) {
  writeBuffer(flow_file, as_bytes(buffer));
}

void ProcessSession::writeBuffer(FlowFile& flow_file, std::span<const std::byte> buffer) {
  write(flow_file, [buffer](const std::shared_ptr<io::OutputStream>& output_stream) -> io::IoResult {
    const auto write_status = output_stream->write(buffer);
    return io::IoResult::from(write_status);
  });
}

std::vector<std::byte> ProcessSession::readBuffer(FlowFile& flow_file) {
  std::vector<std::byte> result;
  read(flow_file, [&result](const std::shared_ptr<io::InputStream>& input_stream) -> io::IoResult {
    result.resize(input_stream->size());
    const auto read_status = input_stream->read(result);
    return io::IoResult::from(read_status);
  });
  return result;
}

}  // namespace org::apache::nifi::minifi::api::core
