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
#ifndef LIBMINIFI_INCLUDE_UTILS_BaseHTTPClient_H_
#define LIBMINIFI_INCLUDE_UTILS_BaseHTTPClient_H_
#include "ByteArrayCallback.h"
#include "controllers/SSLContextService.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

struct HTTPProxy {
  std::string host;
  std::string username;
  std::string password;
  int port;
};

struct HTTPUploadCallback {
  HTTPUploadCallback() {
    stop = false;
    ptr = nullptr;
    pos = 0;
  }
  std::mutex mutex;
  std::atomic<bool> stop;
  ByteInputCallBack *ptr;
  size_t pos;

  size_t getPos() {
    std::lock_guard<std::mutex> lock(mutex);
    return pos;
  }
};

struct HTTPReadCallback {
  HTTPReadCallback() {
    stop = false;
    ptr = nullptr;
    pos = 0;
  }
  std::mutex mutex;
  std::atomic<bool> stop;
  ByteOutputCallback *ptr;
  size_t pos;

  size_t getPos() {
    std::lock_guard<std::mutex> lock(mutex);
    return pos;
  }
};

struct HTTPHeaderResponse {
 public:

  HTTPHeaderResponse(int max)
      : max_tokens_(max) {
  }

  void append(const std::string &header) {
    if (max_tokens_ == -1 || (int32_t)header_tokens_.size() <= max_tokens_) {
      header_tokens_.push_back(header);
    }
  }

  void append(const std::string &key, const std::string &value) {
    header_mapping_[key].append(value);
  }

  int32_t max_tokens_;
  std::vector<std::string> header_tokens_;
  std::map<std::string, std::string> header_mapping_;

  static size_t receive_headers(void *buffer, size_t size, size_t nmemb, void *userp) {
    HTTPHeaderResponse *pHeaders = (HTTPHeaderResponse *) (userp);
    int result = 0;
    if (pHeaders != NULL) {
      std::string s = "";
      s.append((char*) buffer, size * nmemb);
      pHeaders->append(s);
      result = size * nmemb;
    }
    return result;
  }
};

/**
 * HTTP Response object
 */
class HTTPRequestResponse {

  std::vector<char> data;
  std::condition_variable space_available_;
  std::mutex data_mutex_;

  size_t max_queue;

 public:

  const std::vector<char> &getData() {
    return data;
  }

  HTTPRequestResponse(const HTTPRequestResponse &other)
      : max_queue(other.max_queue) {

  }

  HTTPRequestResponse(size_t max)
      : max_queue(max) {

  }
  /**
   * Receive HTTP Response.
   */
  static size_t recieve_write(char * data, size_t size, size_t nmemb, void * p) {
    HTTPReadCallback *callback = static_cast<HTTPReadCallback*>(p);
    if (callback->stop)
      return 0x10000000;
    callback->ptr->write(data, (size * nmemb));
    return (size * nmemb);
  }

  /**
   * Callback for post, put, and patch operations
   * @param buffer
   * @param size size of buffer
   * @param nitems items to add
   * @param insteam input stream object.
   */

  static size_t send_write(char * data, size_t size, size_t nmemb, void * p) {
    if (p != 0) {
      HTTPUploadCallback *callback = (HTTPUploadCallback*) p;
      if (callback->stop)
        return 0x10000000;
      size_t buffer_size = callback->ptr->getBufferSize();
      if (callback->getPos() <= buffer_size) {
        size_t len = buffer_size - callback->pos;
        if (len <= 0)
        {
          return 0;
        }
        char *ptr = callback->ptr->getBuffer(callback->getPos());

        if (ptr == nullptr) {
          return 0;
        }
        if (len > size * nmemb)
          len = size * nmemb;
        auto strr = std::string(ptr,len);
        memcpy(data, ptr, len);
        callback->pos += len;
        callback->ptr->seek(callback->getPos());
        return len;
      }
    } else {
      return 0x10000000;
    }
    return 0;
  }

  int read_data(uint8_t *buf, size_t size) {
    size_t size_to_read = size;
    if (size_to_read > data.size()) {
      size_to_read = data.size();
    }
    memcpy(buf, data.data(), size_to_read);
    return size_to_read;
  }

  size_t write_content(char* ptr, size_t size, size_t nmemb) {

    if (data.size() + (size * nmemb) > max_queue) {
      std::unique_lock<std::mutex> lock(data_mutex_);
      space_available_.wait(lock, [&] {return data.size() + (size*nmemb) < max_queue;});
    }
    data.insert(data.end(), ptr, ptr + size * nmemb);
    return size * nmemb;
  }

};

class BaseHTTPClient {
public:
  explicit BaseHTTPClient(const std::string &url, const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr) {
    response_code = -1;
  }

  explicit BaseHTTPClient() {
    response_code = -1;
  }

  virtual ~BaseHTTPClient() {
  }

  virtual void setVerbose() {
  }

  virtual void initialize(const std::string &method, const std::string url = "", const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr) {
  }

  virtual void setConnectionTimeout(int64_t timeout) {
  }

  virtual void setReadTimeout(int64_t timeout) {
  }

  virtual void setUploadCallback(HTTPUploadCallback *callbackObj) {
  }

  virtual void setContentType(std::string content_type) {
  }

  virtual std::string escape(std::string string_to_escape) {
    return "";
  }

  virtual void setPostFields(std::string input) {
  }

  virtual bool submit() {
    return false;
  }

  virtual int64_t &getResponseCode() {
    return response_code;
  }

  virtual const char *getContentType() {
    return "";
  }

  virtual const std::vector<char> &getResponseBody() {
    return response_body_;
  }

  virtual void appendHeader(const std::string &new_header) {

  }

  virtual void set_request_method(const std::string method) {
  }

  virtual void setUseChunkedEncoding() {
  }

  virtual void setDisablePeerVerification() {
  }

  virtual void setHTTPProxy(const utils::HTTPProxy &proxy) {
  }

  virtual void setDisableHostVerification() {
  }

  virtual const std::vector<std::string> &getHeaders() {
    return headers_;

  }

  virtual const std::map<std::string, std::string> &getParsedHeaders() {
    return header_mapping_;
  }

 protected:
  int64_t response_code;
  std::vector<char> response_body_;
  std::vector<std::string> headers_;
  std::map<std::string, std::string> header_mapping_;

  virtual inline bool matches(const std::string &value, const std::string &sregex) {
    return false;
  }

};

extern std::string get_token(utils::BaseHTTPClient *client, std::string username, std::string password);

extern void parse_url(std::string *url, std::string *host, int *port, std::string *protocol);
extern void parse_url(std::string *url, std::string *host, int *port, std::string *protocol, std::string *path, std::string *query);
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_BaseHTTPClient_H_ */
