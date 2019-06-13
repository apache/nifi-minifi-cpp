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

#ifndef LIBMINIFI_INCLUDE_IO_PASSTHROUGHMEMORYMAP_H_
#define LIBMINIFI_INCLUDE_IO_PASSTHROUGHMEMORYMAP_H_
#include <cstdint>
#include <functional>
#include <iostream>
#include "BaseMemoryMap.h"
#include "Serializable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * DatabaseMemoryMap allows access to an existing underlying memory buffer.
 */
class DatabaseMemoryMap : public BaseMemoryMap {
 public:
  DatabaseMemoryMap(const std::shared_ptr<minifi::ResourceClaim> &claim, size_t map_size, std::function<std::shared_ptr<io::BaseStream>(const std::shared_ptr<minifi::ResourceClaim> &)> write_fn, bool read_only) : claim_(claim), write_fn_(write_fn), read_only_(read_only) {
    buf.resize(map_size);
  }

  virtual ~DatabaseMemoryMap() { unmap(); }

  /**
   * Gets a the address of the mapped data.
   * @return pointer to the mapped data, or nullptr if not mapped
   **/
  virtual void *getData() {
    return reinterpret_cast<void *>(&buf[0]);
  }

  /**
   * Gets the size of the memory map.
   * @return size of memory map
   */
  virtual size_t getSize() { return buf.size(); }

  /**
   * Resize the underlying buffer.
   * @return pointer to the remapped data
   */
  virtual void *resize(size_t new_size) {
    buf.resize(new_size);
    return reinterpret_cast<void *>(&buf[0]);
  }

  /**
   * Explicitly unmap the memory. Memory will otherwise be unmapped at
   * destruction. After this is called, getData will return nullptr.
   */
  virtual void unmap() {
    if (!read_only_) {
      commit();
    }
  }

  /**
   * Commits the changes in memory to the underlying DB.
   */
  void commit() {
    auto ws = write_fn_(claim_);
    if (ws->writeData(&buf[0], getSize()) != 0) {
      throw std::runtime_error("Failed to write memory map data to db: " + claim_->getContentFullPath());
    }
  }

 protected:
  std::vector<uint8_t> buf;
  std::shared_ptr<minifi::ResourceClaim> claim_;
  std::function<std::shared_ptr<io::BaseStream>(const std::shared_ptr<minifi::ResourceClaim> &)> write_fn_;
  bool read_only_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_IO_PASSTHROUGHMEMORYMAP_H_ */
