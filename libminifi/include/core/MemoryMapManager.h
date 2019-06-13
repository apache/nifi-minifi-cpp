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
#ifndef LIBMINIFI_INCLUDE_CORE_MEMORYMAPMANAGER_H_
#define LIBMINIFI_INCLUDE_CORE_MEMORYMAPMANAGER_H_

#include "io/BaseMemoryMap.h"

#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Purpose: Provides a base for all memory-mapping managers. The goal here is to
 * provide a small set of interfaces that provide a small set of operations to
 * provide state management of memory maps.
 */
template <typename T>
class MemoryMapManager {
 public:
  virtual ~MemoryMapManager() {}

  /**
   * Create a memory map to the object.
   * @param map_obj the object to map
   * @return result of operation (true of succeeded)
   */
  virtual std::shared_ptr<io::BaseMemoryMap> mmap(const std::shared_ptr<T> &mapObj, size_t mapSize, bool readOnly) = 0;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_MEMORYMAPMANAGER_H_ */
