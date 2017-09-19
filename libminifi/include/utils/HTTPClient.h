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
#include "ByteInputCallBack.h"
#include "controllers/SSLContextService.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

struct HTTPUploadCallback {
  ByteInputCallBack *ptr;
  size_t pos;
};

struct HTTPHeaderResponse {
 public:

  HTTPHeaderResponse(int max)
      : max_tokens_(max) {
  }

  void append(const std::string &header) {
    if (header_tokens_.size() <= max_tokens_) {
      header_tokens_.push_back(header);
    }
  }

  int max_tokens_;
  std::vector<std::string> header_tokens_;

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
struct HTTPRequestResponse {
  std::vector<char> data;

  /**
   * Receive HTTP Response.
   */
  static size_t recieve_write(char * data, size_t size, size_t nmemb, void * p) {
    return static_cast<HTTPRequestResponse*>(p)->write_content(data, size, nmemb);
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
      if (callback->pos <= callback->ptr->getBufferSize()) {
        char *ptr = callback->ptr->getBuffer();
        int len = callback->ptr->getBufferSize() - callback->pos;
        if (len <= 0) {
          return 0;
        }
        if (len > size * nmemb)
          len = size * nmemb;
        memcpy(data, callback->ptr->getBuffer() + callback->pos, len);
        callback->pos += len;
        return len;
      }
    } else {
      return 0x10000000;
    }

    return 0;
  }

  size_t write_content(char* ptr, size_t size, size_t nmemb) {
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

  virtual const std::vector<std::string> &getHeaders() {
    return headers_;

  }

 protected:
  int64_t response_code;
  std::vector<char> response_body_;
  std::vector<std::string> headers_;

  virtual inline bool matches(const std::string &value, const std::string &sregex){
    return false;
  }

};

static std::string get_token(utils::BaseHTTPClient *client, std::string username, std::string password) {

  if (nullptr == client) {
    return "";
  }
  utils::HTTPRequestResponse content;
  std::string token;

  client->setContentType("application/x-www-form-urlencoded");

  client->set_request_method("POST");

  std::string payload = "username=" + username + "&" + "password=" + password;

  client->setPostFields(client->escape(payload));

  client->submit();

  if (client->submit() && client->getResponseCode() == 200) {

    const std::string &response_body = std::string(client->getResponseBody().data(), client->getResponseBody().size());

    if (!response_body.empty()) {
      token = "Bearer " + response_body;
    }
  }

  return token;
}

static void parse_url(std::string &url, std::string &host, int &port, std::string &protocol) {

  std::string http("http://");
  std::string https("https://");

  if (url.compare(0, http.size(), http) == 0)
    protocol = http;

  if (url.compare(0, https.size(), https) == 0)
    protocol = https;

  if (!protocol.empty()) {
    size_t pos = url.find_first_of(":", protocol.size());

    if (pos == std::string::npos) {
      pos = url.size();
    }

    host = url.substr(protocol.size(), pos - protocol.size());

    if (pos < url.size() && url[pos] == ':') {
      size_t ppos = url.find_first_of("/", pos);
      if (ppos == std::string::npos) {
        ppos = url.size();
      }
      std::string portStr(url.substr(pos + 1, ppos - pos - 1));
      if (portStr.size() > 0) {
        port = std::stoi(portStr);
      }
    }
  }
}
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_BaseHTTPClient_H_ */
