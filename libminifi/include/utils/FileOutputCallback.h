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
#ifndef LIBMINIFI_INCLUDE_UTILS_FILEOUTPUTCALLBACK_H_
#define LIBMINIFI_INCLUDE_UTILS_FILEOUTPUTCALLBACK_H_

#include <fstream>
#include "concurrentqueue.h"
#include "FlowFileRecord.h"
#include "ByteArrayCallback.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * General vector based uint8_t callback.
 *
 * While calls are thread safe, the class is intended to have
 * a single consumer.
 */
class FileOutputCallback : public ByteOutputCallback {
 public:
  FileOutputCallback() = delete;

  explicit FileOutputCallback(std::string file, bool wait_on_read=false)
      : ByteOutputCallback(INT_MAX), file_(file), file_stream_(file),
        logger_(logging::LoggerFactory<FileOutputCallback>::getLogger()) {
  }

  virtual ~FileOutputCallback() {

  }

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) override;

  virtual const std::vector<char> to_string() override;

  virtual void close() override;

  virtual size_t getSize() override;

  virtual void write(char *data, size_t size) override;

 private:

  std::string file_;

  std::ofstream file_stream_;

  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_BYTEARRAYCALLBACK_H_ */
