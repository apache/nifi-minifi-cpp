/**
 * HTTPUtils class declaration
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
#include "controllers/SSLContextService.h"
#include "utils/ByteInputCallBack.h"
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

  void setVerbose();

  void initialize(const std::string &method, const std::string url = "", const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr);

  void setConnectionTimeout(int64_t timeout);

  void setReadTimeout(int64_t timeout);

  void setUploadCallback(HTTPUploadCallback *callbackObj);

  struct curl_slist *build_header_list(std::string regex, const std::map<std::string, std::string> &attributes);

  void setContentType(std::string content_type);

  std::string escape(std::string string_to_escape);

  void setPostFields(std::string input);

  void setHeaders(struct curl_slist *list);

  void appendHeader(const std::string &new_header);

  bool submit();

  CURLcode getResponseResult();

  int64_t &getResponseCode();

  const char *getContentType();

  const std::vector<char> &getResponseBody();

  void set_request_method(const std::string method);

  void setUseChunkedEncoding();

  void setDisablePeerVerification();

  const std::vector<std::string> &getHeaders() {
    return header_response_.header_tokens_;

  }

  /**
   * Determines if we are connected and operating
   */
  virtual bool isRunning() {
    return true;
  }

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(uint64_t timeoutMs) {
  }

  virtual void yield() {

  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  virtual bool isWorkAvailable() {
    return true;
  }

 protected:

  inline bool matches(const std::string &value, const std::string &sregex);

  static CURLcode configure_ssl_context(CURL *curl, void *ctx, void *param) {
    minifi::controllers::SSLContextService *ssl_context_service = static_cast<minifi::controllers::SSLContextService*>(param);
    if (!ssl_context_service->configure_ssl_context(static_cast<SSL_CTX*>(ctx))) {
      return CURLE_FAILED_INIT;
    }
    return CURLE_OK;
  }

  void configure_secure_connection(CURL *http_session);

  bool isSecure(const std::string &url);
  struct curl_slist *headers_;
  utils::HTTPRequestResponse content_;
  utils::HTTPHeaderResponse header_response_;
  CURLcode res;
  int64_t http_code;
  char *content_type;

  int64_t connect_timeout_;
// read timeout.
  int64_t read_timeout_;

  std::string content_type_;

  std::shared_ptr<logging::Logger> logger_;
  CURL *http_session_;
  std::string url_;
  std::string method_;
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
