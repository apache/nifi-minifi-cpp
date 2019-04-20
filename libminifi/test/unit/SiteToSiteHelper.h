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
#ifndef LIBMINIFI_TEST_UNIT_SITE2SITE_HELPER_H_
#define LIBMINIFI_TEST_UNIT_SITE2SITE_HELPER_H_

#include <queue>
#include "io/BaseStream.h"
#include "io/EndianCheck.h"
#include "core/Core.h"
/**
 * Test repository
 */
class SiteToSiteResponder : public minifi::io::BaseStream {
 private:
  std::queue<std::string> server_responses_;
  std::queue<std::string> client_responses_;
 public:
  SiteToSiteResponder() {
  }
  // initialize
  virtual short initialize() {
    return 1;
  }

  void push_response(std::string resp) {
    server_responses_.push(resp);
  }

  std::string get_next_response() {
    std::string ret = server_responses_.front();
    server_responses_.pop();
    return ret;
  }

  int writeData(uint8_t *value, int size) {
    client_responses_.push(std::string((char*) value, size));
    return size;
  }

  bool has_next_client_response() {
    return !client_responses_.empty();
  }

  std::string get_next_client_response() {
    std::string ret = client_responses_.front();
    client_responses_.pop();
    return ret;
  }

  /**
   * reads a byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint8_t &value) {
    value = get_next_response().c_str()[0];
    return 1;
  }

  /**
   * reads two bytes from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint16_t &base_value, bool is_little_endian = minifi::io::EndiannessCheck::IS_LITTLE) {
    base_value = std::stoi(get_next_response());
    return 2;
  }

  /**
   * reads a byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(char &value) {
    value = get_next_response().c_str()[0];
    return 1;
  }

  /**
   * reads a byte array from the stream
   * @param value reference in which will set the result
   * @param len length to read
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint8_t *value, int len) {
    std::string str = get_next_response();
    memcpy(value, str.c_str(), str.size());
    return len;
  }

  virtual int readData(uint8_t *buf, int buflen) {
    std::string str = get_next_response();
    memset(buf, 0x00, buflen);
    memcpy(buf, str.c_str(), str.size());
    return str.size();
  }

  /**
   * reads four bytes from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint32_t &value, bool is_little_endian = minifi::io::EndiannessCheck::IS_LITTLE) {
    value = std::stoul(get_next_response());
    return 4;
  }

  /**
   * reads eight byte from the stream
   * @param value reference in which will set the result
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int read(uint64_t &value, bool is_little_endian = minifi::io::EndiannessCheck::IS_LITTLE) {
    value = std::stoull(get_next_response());
    return 8;
  }

  /**
   * read UTF from stream
   * @param str reference string
   * @param stream stream from which we will read
   * @return resulting read size
   **/
  virtual int readUTF(std::string &str, bool widen = false) {
    str = get_next_response();
    return str.length();
  }

};

#endif /* LIBMINIFI_TEST_UNIT_SITE2SITE_HELPER_H_ */
