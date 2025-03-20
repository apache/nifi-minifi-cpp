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

#include "core/repository/VolatileContentRepository.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core::repository {

namespace {

class StringRefStream : public io::BaseStream {
 public:
  StringRefStream(std::shared_ptr<std::string> data, std::mutex& data_store_mtx, std::shared_ptr<std::string>& data_store, std::atomic<size_t>& total_size)
      : data_(std::move(data)), data_store_mtx_(data_store_mtx), data_store_(data_store), total_size_(total_size) {}

  [[nodiscard]] size_t size() const override {
    return data_->size();
  }

  size_t read(std::span<std::byte> out_buffer) override {
    auto read_size = std::min(data_->size() - read_offset_, out_buffer.size());
    std::copy_n(reinterpret_cast<const std::byte*>(data_->data()) + read_offset_, read_size, out_buffer.data());
    read_offset_ += read_size;
    return read_size;
  }

  size_t write(const uint8_t *value, size_t len) override {
    data_ = std::make_shared<std::string>(*data_);
    data_->append(reinterpret_cast<const char*>(value), len);
    total_size_ += len;
    {
      std::lock_guard lock(data_store_mtx_);
      data_store_ = data_;
    }
    return len;
  }

  void close() override {}

  void seek(size_t offset) override {
    read_offset_ = std::min(offset, data_->size());
  }

  size_t tell() const override {
    return read_offset_;
  }

  int initialize() override {
    return 1;
  }

  std::span<const std::byte> getBuffer() const override {
    return as_bytes(std::span{*data_});
  }

 private:
  size_t read_offset_{0};
  std::shared_ptr<std::string> data_;
  std::mutex& data_store_mtx_;
  std::shared_ptr<std::string>& data_store_;
  std::atomic<size_t>& total_size_;
};

}  // namespace

VolatileContentRepository::VolatileContentRepository(std::string_view name)
  : ContentRepositoryImpl(name),
    logger_(logging::LoggerFactory<VolatileContentRepository>::getLogger()) {}

uint64_t VolatileContentRepository::getRepositorySize() const {
  return total_size_.load();
}

uint64_t VolatileContentRepository::getMaxRepositorySize() const {
  return std::numeric_limits<uint64_t>::max();
}

uint64_t VolatileContentRepository::getRepositoryEntryCount() const {
  std::lock_guard lock(data_mtx_);
  return data_.size();
}

bool VolatileContentRepository::isFull() const {
  return false;
}

bool VolatileContentRepository::initialize(const std::shared_ptr<Configure>& /*configure*/) {
  return true;
}

std::shared_ptr<io::BaseStream> VolatileContentRepository::write(const minifi::ResourceClaim &claim, bool append) {
  std::lock_guard lock(data_mtx_);
  auto& value_ref = data_[claim.getContentFullPath()];
  if (!value_ref) {
    value_ref = std::make_shared<std::string>();
  } else if (!append) {
    total_size_ -= value_ref->size();
    value_ref = std::make_shared<std::string>();
  }
  return std::make_shared<StringRefStream>(value_ref, data_mtx_, value_ref, total_size_);
}

std::shared_ptr<io::BaseStream> VolatileContentRepository::read(const minifi::ResourceClaim &claim) {
  std::lock_guard lock(data_mtx_);
  if (auto it = data_.find(claim.getContentFullPath()); it != data_.end()) {
    return std::make_shared<StringRefStream>(it->second, data_mtx_, it->second, total_size_);
  }
  return nullptr;
}

bool VolatileContentRepository::exists(const minifi::ResourceClaim &claim) {
  std::lock_guard lock(data_mtx_);
  return data_.contains(claim.getContentFullPath());
}

bool VolatileContentRepository::close(const minifi::ResourceClaim &claim) {
  return remove(claim);
}

void VolatileContentRepository::clearOrphans() {
  // there are no persisted orphans to delete
}

bool VolatileContentRepository::removeKey(const std::string& content_path) {
  std::lock_guard lock(data_mtx_);
  if (auto it = data_.find(content_path); it != data_.end()) {
    total_size_ -= it->second->size();
    data_.erase(it);
    logger_->log_info("Deleting resource {}", content_path);
  }

  return true;
}

}  // namespace org::apache::nifi::minifi::core::repository
