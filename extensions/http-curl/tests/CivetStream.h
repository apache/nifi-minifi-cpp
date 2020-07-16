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
#ifndef EXTENSIONS_HTTP_CURL_CLIENT_CIVETSTREAM_H_
#define EXTENSIONS_HTTP_CURL_CLIENT_CIVETSTREAM_H_

#include <algorithm>
#include <memory>
#include <thread>
#include <mutex>
#include <future>
#include <vector>

#include "io/BaseStream.h"
#include "civetweb.h"
#include "CivetServer.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class CivetStream : public io::InputStream {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit CivetStream(struct mg_connection *conn) : conn(conn) {}

  ~CivetStream() override = default;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  int read(uint8_t *buf, int buflen) override {
    return mg_read(conn, buf, buflen);
  }

 protected:
  struct mg_connection *conn;

 private:

  std::shared_ptr<logging::Logger> logger_;
};
} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_HTTP_CURL_CLIENT_CIVETSTREAM_H_ */
