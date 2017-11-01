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

class CivetStream : public io::BaseStream {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit CivetStream(struct mg_connection *conn)
      : io::BaseStream(), conn(conn) {

  }

  virtual ~CivetStream() {
  }
  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(uint64_t offset){

  }

  const uint64_t getSize() const {
    return BaseStream::readBuffer;
  }

  // data stream extensions
  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(std::vector<uint8_t> &buf, int buflen) {
    if (buf.capacity() < buflen) {
      buf.resize(buflen);
    }
    int ret = readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen);

    if (ret < buflen) {
      buf.resize(ret);
    }
    return ret;
  }

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  virtual int readData(uint8_t *buf, int buflen) {
    return mg_read(conn,buf,buflen);
  }

  /**
   * Write value to the stream using std::vector
   * @param buf incoming buffer
   * @param buflen buffer to write
   *
   */
  virtual int writeData(std::vector<uint8_t> &buf, int buflen) {
    return 0;
  }

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  virtual int writeData(uint8_t *value, int size) {
    return 0;
  }

 protected:

  /**
   * Creates a vector and returns the vector using the provided
   * type name.
   * @param t incoming object
   * @returns vector.
   */
  template<typename T>
  inline std::vector<uint8_t> readBuffer(const T& t) {
    std::vector<uint8_t> buf;
    buf.resize(sizeof t);
    readData(reinterpret_cast<uint8_t *>(&buf[0]), sizeof(t));
    return buf;
  }

  void reset();

  //size_t pos;
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
