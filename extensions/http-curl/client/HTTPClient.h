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
#ifndef __HTTP_UTILS_H__
#define __HTTP_UTILS_H__

#include "utils/HTTPClient.h"
#include <curl/curl.h>
#include <vector>
#include <iostream>
#include <string>
#include <curl/easy.h>
#include <uuid/uuid.h>
#include <regex.h>
#include <vector>

#include "utils/ByteArrayCallback.h"
#include "controllers/SSLContextService.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/Configure.h"
#include "io/validation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * Purpose and Justification: Initializes and cleans up curl once. Cleanup will only occur at the end of our execution since we are relying on a static variable.
 */
class HTTPClientInitializer {
 public:
  static HTTPClientInitializer *getInstance() {
    static HTTPClientInitializer initializer;
    return &initializer;
  }
  void initialize() {

  }
 private:
  ~HTTPClientInitializer() {
    curl_global_cleanup();
  }
  HTTPClientInitializer() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }
};

/**
 * Purpose and Justification: Pull the basics for an HTTPClient into a self contained class. Simply provide
 * the URL and an SSLContextService ( can be null).
 *
 * Since several portions of the code have been relying on curl, we can encapsulate most CURL HTTP
 * operations here without maintaining it everywhere. Further, this will help with testing as we
 * only need to to test our usage of CURL once
 */
class HTTPClient : public BaseHTTPClient, public core::Connectable {
 public:

  HTTPClient();

  HTTPClient(std::string name, uuid_t uuid);

  HTTPClient(const std::string &url, const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr);

  ~HTTPClient();

  virtual void setVerbose() override;

  void forceClose();

  virtual void initialize(const std::string &method, const std::string url = "", const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr) override;

  virtual void setConnectionTimeout(int64_t timeout) override;

  virtual void setReadTimeout(int64_t timeout) override;

  virtual void setUploadCallback(HTTPUploadCallback *callbackObj) override;

  virtual void setReadCallback(HTTPReadCallback *callbackObj);

  struct curl_slist *build_header_list(std::string regex, const std::map<std::string, std::string> &attributes);

  virtual void setContentType(std::string content_type) override;

  virtual std::string escape(std::string string_to_escape) override;

  virtual void setPostFields(std::string input) override;

  void setHeaders(struct curl_slist *list);

  virtual void appendHeader(const std::string &new_header) override;

  void appendHeader(const std::string &key, const std::string &value);

  bool submit() override;

  CURLcode getResponseResult();

  int64_t &getResponseCode() override;

  const char *getContentType() override;

  const std::vector<char> &getResponseBody() override;

  void set_request_method(const std::string method) override;

  void setUseChunkedEncoding() override;

  void setDisablePeerVerification() override;

  void setDisableHostVerification() override;

  std::string getURL() const{
    return url_;
  }

  void setInterface(const std::string &interface) {
    curl_easy_setopt(http_session_, CURLOPT_INTERFACE, interface.c_str());
  }

  const std::vector<std::string> &getHeaders() override {
    return header_response_.header_tokens_;

  }

  virtual const std::map<std::string, std::string> &getParsedHeaders() override {
    return header_response_.header_mapping_;
  }

  /**
   * Determines if we are connected and operating
   */
  virtual bool isRunning() override {
    return true;
  }

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(uint64_t timeoutMs) {
  }

  virtual void yield() override {

  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  virtual bool isWorkAvailable() override {
    return true;
  }

  void setPostSize(size_t size);

 protected:

  inline bool matches(const std::string &value, const std::string &sregex) override;

  static CURLcode configure_ssl_context(CURL *curl, void *ctx, void *param) {
    minifi::controllers::SSLContextService *ssl_context_service = static_cast<minifi::controllers::SSLContextService*>(param);
    if (!ssl_context_service->configure_ssl_context(static_cast<SSL_CTX*>(ctx))) {
      return CURLE_FAILED_INIT;
    }
    return CURLE_OK;
  }

  void configure_secure_connection(CURL *http_session);

  bool isSecure(const std::string &url);

  HTTPReadCallback content_;

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;
  std::string url_;
  int64_t connect_timeout_;
  // read timeout.
  int64_t read_timeout_;
  char *content_type_str_;
  std::string content_type_;
  struct curl_slist *headers_;
  HTTPReadCallback *callback;
  HTTPUploadCallback *write_callback_;
  int64_t http_code;
  ByteOutputCallback read_callback_;
  utils::HTTPHeaderResponse header_response_;

  CURLcode res;

  CURL *http_session_;

  std::string method_;

  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
