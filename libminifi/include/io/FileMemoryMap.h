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
#ifndef LIBMINIFI_INCLUDE_IO_TLS_FILEMEMORYMAP_H_
#define LIBMINIFI_INCLUDE_IO_TLS_FILEMEMORYMAP_H_

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <unistd.h>
#endif

#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <mio/mmap.hpp>
#include "BaseMemoryMap.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * Purpose: File Memory Map extension.
 *
 * Design: Memory maps file in construction, unmaps in destruction (RAII).
 * Provides an interface to get the memory address and size.
 */
class FileMemoryMap : public io::BaseMemoryMap {
 public:
  /**
   * File Memory Map constructor that opens and maps the given file with the
   * given size.
   */
  FileMemoryMap(const std::string &path, size_t map_size, bool read_only);

  virtual ~FileMemoryMap() { unmap(); }

  /**
   * Gets a the address of the mapped data.
   * @return pointer to the mapped data, or nullptr if not mapped
   **/
  virtual void *getData();

  /**
   * Gets the size of the memory map.
   * @return size of memory map
   */
  virtual size_t getSize();

  /**
   * Resize the underlying file.
   * @return pointer to the remapped data
   */
  virtual void *resize(size_t new_size);

  /**
   * Explicitly unmap the memory. Memory will otherwise be unmapped at
   * destruction. After this is called, getData will return nullptr.
   */
  virtual void unmap();

 protected:

  void map(const std::string &path, size_t map_size, bool read_only);

#if defined(_POSIX_VERSION)
  int unix_fd_;
#endif
  size_t length_;
  mio::mmap_sink rw_mmap_;
  mio::mmap_source ro_mmap_;
  std::string path_;
  bool read_only_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_TLS_FILEMEMORYMAP_H_ */
