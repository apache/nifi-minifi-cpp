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

#ifndef LIBMINIFI_INCLUDE_IO_BASEMEMORYMAP_H_
#define LIBMINIFI_INCLUDE_IO_BASEMEMORYMAP_H_
#include <cstdint>
#include <iostream>
#include "Serializable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * BaseMemoryMap is a generic interface to a chunk of memory mapped to an object (e.g. repository data).
 *
 * ** Not intended to be thread safe as it is not intended to be shared**
 *
 * Extensions may be thread safe and thus shareable, but that is up to the implementation.
 */
class BaseMemoryMap {
 public:
  virtual ~BaseMemoryMap() {}

  /**
   * Gets a the address of the mapped data.
   * @return pointer to the mapped data, or nullptr if not mapped
   **/
  virtual void *getData() = 0;

  /**
   * Gets the size of the memory map.
   * @return size of memory map
   */
  virtual size_t getSize() = 0;

  /**
   * Resize the underlying object.
   * @return pointer to the remapped data
   */
  virtual void *resize(size_t newSize) = 0;

  /**
   * Explicitly unmap the memory. Memory will otherwise be unmapped at destruction.
   * After this is called, getData will return nullptr.
   */
  virtual void unmap() = 0;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* LIBMINIFI_INCLUDE_IO_BASEMEMORYMAP_H_ */
