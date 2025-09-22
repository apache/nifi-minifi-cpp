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

std::shared_ptr<minifi::core::IFlowFile> ProcessSession::popFlowFile() {
  return std::make_shared<FlowFile>(MinifiProcessSessionGet(impl_));
}

std::shared_ptr<minifi::core::IFlowFile> ProcessSession::create(const minifi::core::IFlowFile* parent) {
  return std::make_shared<FlowFile>(MinifiProcessSessionCreate(impl_, parent ? dynamic_cast<const FlowFile*>(parent)->getImpl() : MINIFI_NULL));
}

void ProcessSession::transfer(const std::shared_ptr<minifi::core::IFlowFile>& ff, const minifi::core::Relationship& relationship) {
  const auto rel_name = relationship.getName();
  MinifiProcessSessionTransfer(impl_, dynamic_cast<FlowFile*>(ff.get())->getImpl(), utils::toStringView(rel_name));
}

void ProcessSession::write(minifi::core::IFlowFile& flow_file, const io::OutputStreamCallback& callback) {
  auto status = MinifiProcessSessionWrite(impl_, dynamic_cast<FlowFile&>(flow_file).getImpl(), [] (void* data, MinifiOutputStream output) {
    return (*reinterpret_cast<const io::OutputStreamCallback*>(data))(std::make_shared<MinifiOutputStreamWrapper>(output));
  }, (void*)&callback);
  if (status != MINIFI_SUCCESS) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
  }
}

void ProcessSession::read(minifi::core::IFlowFile& flow_file, const io::InputStreamCallback& callback) {
  auto status = MinifiProcessSessionRead(impl_, dynamic_cast<FlowFile&>(flow_file).getImpl(), [] (void* data, MinifiInputStream input) {
    return (*reinterpret_cast<const io::InputStreamCallback*>(data))(std::make_shared<MinifiInputStreamWrapper>(input));
  }, (void*)&callback);
  if (status != MINIFI_SUCCESS) {
    throw minifi::Exception(minifi::FILE_OPERATION_EXCEPTION, "Failed to process flowfile content");
  }
}

void ProcessSession::setAttribute(minifi::core::IFlowFile& ff, std::string_view key, std::string value) {
  MinifiStringView value_ref = utils::toStringView(value);
  MinifiFlowFileSetAttribute(impl_, dynamic_cast<FlowFile&>(ff).getImpl(), utils::toStringView(key), &value_ref);
}

void ProcessSession::removeAttribute(minifi::core::IFlowFile& ff, std::string_view key) {
  MinifiFlowFileSetAttribute(impl_, dynamic_cast<FlowFile&>(ff).getImpl(), utils::toStringView(key), nullptr);
}

std::optional<std::string> ProcessSession::getAttribute(minifi::core::IFlowFile& ff, std::string_view key) {
  std::optional<std::string> result;
  MinifiFlowFileGetAttribute(impl_, dynamic_cast<FlowFile&>(ff).getImpl(), utils::toStringView(key), [] (void* user_ctx, MinifiStringView value) {
    *reinterpret_cast<std::optional<std::string>*>(user_ctx) = std::string{value.data, value.length};
  }, (void*)&result);
  return result;
}

std::map<std::string, std::string> ProcessSession::getAttributes(minifi::core::IFlowFile& ff) {
  std::map<std::string, std::string> result;
  MinifiFlowFileGetAttributes(impl_, dynamic_cast<FlowFile&>(ff).getImpl(), [] (void* user_ctx, MinifiStringView value, MinifiStringView key) {
    reinterpret_cast<std::map<std::string, std::string>*>(user_ctx)->insert({std::string{value.data, value.length}, std::string{key.data, key.length}});
  }, (void*)&result);
  return result;
}

}  // namespace org::apache::nifi::minifi::cpp::core

