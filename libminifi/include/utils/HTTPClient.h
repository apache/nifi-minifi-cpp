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

#include <curl/curl.h>
#include <vector>
#include <iostream>
#include <string>
#include <curl/easy.h>
#include <uuid/uuid.h>
#include <regex.h>
#include <vector>
#include "controllers/SSLContextService.h"
#include "ByteInputCallBack.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/Configure.h"
#include "io/validation.h"

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
      return CURL_READFUNC_ABORT;
    }

    return 0;
  }

  size_t write_content(char* ptr, size_t size, size_t nmemb) {
    data.insert(data.end(), ptr, ptr + size * nmemb);
    return size * nmemb;
  }

};

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
class HTTPClient {
 public:
  HTTPClient(const std::string &url, const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr);

  ~HTTPClient();

  void setVerbose();

  void initialize(const std::string &method);

  void setConnectionTimeout(int64_t timeout);

  void setReadTimeout(int64_t timeout);

  void setUploadCallback(HTTPUploadCallback *callbackObj);

  struct curl_slist *build_header_list(std::string regex, const std::map<std::string, std::string> &attributes);

  void setContentType(std::string content_type);

  std::string escape(std::string string_to_escape);

  void setPostFields(std::string input);

  void setHeaders(struct curl_slist *list);

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

//static std::string get_token(HTTPClientstd::string loginUrl, std::string username, std::string password, HTTPSecurityConfiguration &securityConfig) {
static std::string get_token(HTTPClient &client, std::string username, std::string password) {
  utils::HTTPRequestResponse content;
  std::string token;

  client.setContentType("application/x-www-form-urlencoded");

  client.set_request_method("POST");

  std::string payload = "username=" + username + "&" + "password=" + password;

  client.setPostFields(client.escape(payload));

  client.submit();

  if (client.submit() && client.getResponseCode() == 200) {

    const std::string &response_body = std::string(client.getResponseBody().data(), client.getResponseBody().size());

    if (!response_body.empty()) {
      token = "Bearer " + response_body;
    }
  }

  return token;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
