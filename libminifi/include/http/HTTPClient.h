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
#pragma once

#include "http/BaseHTTPClient.h"
#ifdef WIN32
#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")
#ifndef CURL_STATICLIB
#define CURL_STATICLIB
#endif
#include <curl/curl.h>
#else
#include <curl/curl.h>
#endif
#include <curl/easy.h>
#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "utils/ByteArrayCallback.h"
#include "controllers/SSLContextService.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::http {

struct KeepAliveProbeData {
  std::chrono::seconds keep_alive_delay;
  std::chrono::seconds keep_alive_interval;
};

struct HTTPResponseData {
  std::vector<char> response_body;
  HTTPHeaderResponse header_response;
  char* response_content_type{nullptr};
  int64_t response_code{0};

  void clear() {
    header_response.clear();
    response_body.clear();
    response_content_type = nullptr;
    response_code = 0;
  }
};

class HTTPClient : public BaseHTTPClient, public core::Connectable {
 public:
  HTTPClient();

  HTTPClient(std::string_view name, const utils::Identifier& uuid);

  HTTPClient(const HTTPClient&) = delete;
  HTTPClient& operator=(const HTTPClient&) = delete;

  explicit HTTPClient(std::string url, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr);

  ~HTTPClient() override;

  MINIFIAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr bool SupportsDynamicRelationships = false;

  static int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr);

  void setVerbose(bool use_stderr) override;

  void addFormPart(const std::string& content_type, const std::string& name, std::unique_ptr<HTTPUploadCallback> form_callback, const std::optional<std::string>& filename);

  void forceClose();

  void initialize(http::HttpRequestMethod method, std::string url, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) override;

  void setConnectionTimeout(std::chrono::milliseconds timeout) override;

  void setReadTimeout(std::chrono::milliseconds timeout) override;

  void setUploadCallback(std::unique_ptr<HTTPUploadCallback> callback) override;

  void setReadCallback(std::unique_ptr<HTTPReadCallback> callback);

  HTTPUploadCallback* getUploadCallback() const { return write_callback_.get(); }
  HTTPReadCallback* getReadCallback() const { return read_callback_.get(); }

  void setContentType(std::string content_type) override;

  std::string escape(std::string string_to_escape) override;

  void setPostFields(const std::string& input) override;

  void setRequestHeader(std::string key, std::optional<std::string> value) override;

  bool submit() override;

  int64_t getResponseCode() const override;

  const char *getContentType() override;

  const std::vector<char>& getResponseBody() override;

  void set_request_method(http::HttpRequestMethod method) override;

  void setPeerVerification(bool peer_verification) override;
  void setHostVerification(bool host_verification) override;

  void setBasicAuth(const std::string& username, const std::string& password) override;
  void clearBasicAuth() override;

  bool setSpecificSSLVersion(SSLVersion specific_version) override;

  bool setMinimumSSLVersion(SSLVersion minimum_version) override;

  void setKeepAliveProbe(std::optional<KeepAliveProbeData> probe_data);

  const std::string& getURL() const {
    return url_;
  }

  const std::vector<std::string>& getResponseHeaders() override {
    return response_data_.header_response.getHeaderLines();
  }

  const std::map<std::string, std::string>& getResponseHeaderMap() override {
    return response_data_.header_response.getHeaderMap();
  }

  std::optional<http::HttpRequestMethod> getMethod() const {
    return method_;
  }

  void setInterface(const std::string &);

  void setFollowRedirects(bool follow);

  // Limit connection throughput in bytes per second
  void setMaximumUploadSpeed(uint64_t max_bytes_per_second);
  void setMaximumDownloadSpeed(uint64_t max_bytes_per_second);

  /**
   * Locates the header value ignoring case. This is different than returning a mapping
   * of all parsed headers.
   * This function acknowledges that header entries should be searched case insensitively.
   * @param key key to search
   * @return header value.
   */
  std::string getHeaderValue(const std::string &key) {
    std::string ret;
    for (const auto &kv : response_data_.header_response.getHeaderMap()) {
      if (utils::string::equalsIgnoreCase(key, kv.first)) {
        ret = kv.second;
        break;
      }
    }
    return ret;
  }

  /**
   * Determines if we are connected and operating
   */
  bool isRunning() const override {
    return true;
  }

  void yield() override {
  }

  bool isWorkAvailable() override {
    return true;
  }

  void setPostSize(size_t size);

  void setHTTPProxy(const HTTPProxy &proxy) override;

  static bool isValidHttpHeaderField(std::string_view field_name);
  static std::string replaceInvalidCharactersInHttpHeaderFieldName(std::string field_name);

 private:
  static int onProgress(void *client, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

  struct Progress{
    std::chrono::steady_clock::time_point last_transferred_;
    curl_off_t uploaded_data_{};
    curl_off_t downloaded_data_{};
    void reset(){
      last_transferred_ = std::chrono::steady_clock::now();
      uploaded_data_ = 0;
      downloaded_data_ = 0;
    }
  };

  Progress progress_;

 protected:
  static CURLcode configure_ssl_context(CURL* /*curl*/, void *ctx, void *param) {
    gsl_Expects(ctx);
    gsl_Expects(param);
    auto& ssl_context_service = *static_cast<minifi::controllers::SSLContextService*>(param);
    if (!ssl_context_service.configure_ssl_context(static_cast<SSL_CTX*>(ctx))) {
      return CURLE_FAILED_INIT;
    }
    return CURLE_OK;
  }

  void configure_secure_connection();

  std::chrono::milliseconds getAbsoluteTimeout() const { return 3*read_timeout_; }

  HTTPReadCallback content_{std::numeric_limits<size_t>::max()};

  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;
  std::string url_;
  std::optional<http::HttpRequestMethod> method_;

  std::chrono::milliseconds connect_timeout_{std::chrono::seconds(30)};
  std::chrono::milliseconds read_timeout_{std::chrono::seconds(30)};

  HTTPResponseData response_data_;

  CURLcode res_{CURLE_OK};

  std::unordered_map<std::string, std::string> request_headers_;

  struct CurlEasyCleanup { void operator()(CURL* curl) const; };
  struct CurlMimeFree { void operator()(curl_mime* curl_mime) const; };

  std::unique_ptr<CURL, CurlEasyCleanup> http_session_;
  std::unique_ptr<curl_mime, CurlMimeFree> form_;
  std::unique_ptr<HTTPReadCallback> read_callback_;
  std::unique_ptr<HTTPUploadCallback> write_callback_;
  std::unique_ptr<HTTPUploadCallback> form_callback_;

  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<HTTPClient>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::http
