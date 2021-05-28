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

// Visual Studio 2017 warns when overriding a deprecated function, even if
// the override is also deprecated.  Note that we need to put this #pragma
// here, because it doesn't work inside the #ifndef
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

#pragma once

#include "utils/HTTPClient.h"
#ifdef WIN32
#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")

#define CURL_STATICLIB
#include <curl/curl.h>
#else
#include <curl/curl.h>
#endif
#include <curl/easy.h>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <string>
#ifdef WIN32
#include <regex>
#else
#include <regex.h>
#endif

#include "utils/ByteArrayCallback.h"
#include "controllers/SSLContextService.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

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

  HTTPClient(const std::string& name, const utils::Identifier& uuid);

  explicit HTTPClient(const std::string &url, const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr);

  ~HTTPClient();

  static int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr);

  void setVerbose(bool use_stderr = false) override;

  void forceClose();

  void initialize(const std::string &method, const std::string url = "", const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr) override;

  // This is a bad API and deprecated. Use the std::chrono variant of this
  // It is assumed that the value of timeout provided to this function
  // is in seconds units
  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) void setConnectionTimeout(int64_t timeout) override;

  // This is a bad API and deprecated. Use the std::chrono variant of this
  // It is assumed that the value of timeout provided to this function
  // is in seconds units
  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) void setReadTimeout(int64_t timeout) override;

  void setConnectionTimeout(std::chrono::milliseconds timeout) override;

  void setReadTimeout(std::chrono::milliseconds timeout) override;

  void setUploadCallback(HTTPUploadCallback *callbackObj) override;

  void setSeekFunction(HTTPUploadCallback *callbackObj) override;

  virtual void setReadCallback(HTTPReadCallback *callbackObj);

  struct curl_slist *build_header_list(std::string regex, const std::map<std::string, std::string> &attributes);

  void setContentType(std::string content_type) override;

  std::string escape(std::string string_to_escape) override;

  void setPostFields(const std::string& input) override;

  void setHeaders(struct curl_slist *list);

  void appendHeader(const std::string &new_header) override;

  void appendHeader(const std::string &key, const std::string &value);

  bool submit() override;

  CURLcode getResponseResult();

  int64_t getResponseCode() const override;

  const char *getContentType() override;

  const std::vector<char> &getResponseBody() override;

  void set_request_method(const std::string method) override;

  void setUseChunkedEncoding() override;

  void setDisablePeerVerification() override;

  void setDisableHostVerification() override;

  bool setSpecificSSLVersion(SSLVersion specific_version) override;

  bool setMinimumSSLVersion(SSLVersion minimum_version) override;

  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) void setKeepAliveProbe(long probe) {  // NOLINT deprecated
    keep_alive_probe_ = std::chrono::milliseconds(probe * 1000);
  }

  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) void setKeepAliveIdle(long idle) {  // NOLINT deprecated
    keep_alive_idle_ = std::chrono::milliseconds(idle * 1000);
  }

  void setKeepAliveProbe(std::chrono::milliseconds probe) {
    keep_alive_probe_ = probe;
  }

  void setKeepAliveIdle(std::chrono::milliseconds idle) {
    keep_alive_idle_ = idle;
  }


  std::string getURL() const {
    return url_;
  }

  const std::vector<std::string> &getHeaders() override {
    return header_response_.getHeaderLines();
  }

  void setInterface(const std::string &);

  void setFollowRedirects(bool follow);

  const std::map<std::string, std::string> &getParsedHeaders() override {
    return header_response_.getHeaderMap();
  }

  /**
   * Locates the header value ignoring case. This is different than returning a mapping
   * of all parsed headers.
   * This function acknowledges that header entries should be searched case insensitively.
   * @param key key to search
   * @return header value.
   */
  const std::string getHeaderValue(const std::string &key) {
    std::string ret;
    for (const auto &kv : header_response_.getHeaderMap()) {
      if (utils::StringUtils::equalsIgnoreCase(key, kv.first)) {
        ret = kv.second;
        break;
      }
    }
    return ret;
  }

  /**
   * Determines if we are connected and operating
   */
  bool isRunning() override {
    return true;
  }

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(uint64_t /*timeoutMs*/) {
  }

  void yield() override {
  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  bool isWorkAvailable() override {
    return true;
  }

  void setPostSize(size_t size);

  void setHTTPProxy(const utils::HTTPProxy &proxy) override {
    if (!proxy.host.empty()) {
      curl_easy_setopt(http_session_, CURLOPT_PROXY, proxy.host.c_str());
      curl_easy_setopt(http_session_, CURLOPT_PROXYPORT, proxy.port);
      if (!proxy.username.empty()) {
        curl_easy_setopt(http_session_, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
        std::string value = proxy.username + ":" + proxy.password;
        curl_easy_setopt(http_session_, CURLOPT_PROXYUSERPWD, value.c_str());
      }
    }
  }

 private:
  static int onProgress(void *client, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

  struct Progress{
    std::chrono::steady_clock::time_point last_transferred_;
    curl_off_t uploaded_data_;
    curl_off_t downloaded_data_;
    void reset(){
      last_transferred_ = std::chrono::steady_clock::now();
      uploaded_data_ = 0;
      downloaded_data_ = 0;
    }
  };

  Progress progress_;

 protected:
  inline bool matches(const std::string &value, const std::string &sregex) override;

  static CURLcode configure_ssl_context(CURL* /*curl*/, void *ctx, void *param) {
#ifdef OPENSSL_SUPPORT
    minifi::controllers::SSLContextService *ssl_context_service = static_cast<minifi::controllers::SSLContextService*>(param);
    if (!ssl_context_service->configure_ssl_context(static_cast<SSL_CTX*>(ctx))) {
      return CURLE_FAILED_INIT;
    }
    return CURLE_OK;
#else
    return CURLE_FAILED_INIT;
#endif
  }

  void configure_secure_connection(CURL *http_session);

  bool isSecure(const std::string &url);

  HTTPReadCallback content_;

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;
  std::string url_;
  std::chrono::milliseconds connect_timeout_ms_{30000};
  // read timeout.
  std::chrono::milliseconds read_timeout_ms_{30000};
  char *content_type_str_{nullptr};
  std::string content_type_;
  struct curl_slist *headers_{nullptr};
  HTTPReadCallback *callback{nullptr};
  HTTPUploadCallback *write_callback_{nullptr};
  int64_t http_code_{0};
  ByteOutputCallback read_callback_{INT_MAX};
  utils::HTTPHeaderResponse header_response_{-1};

  CURLcode res{CURLE_OK};

  CURL *http_session_;

  std::string method_;

  std::chrono::milliseconds keep_alive_probe_{-1};

  std::chrono::milliseconds keep_alive_idle_{-1};

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<HTTPClient>::getLogger()};
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#ifdef WIN32
#pragma warning(pop)
#endif
