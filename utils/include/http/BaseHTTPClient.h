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

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

#include "utils/ByteArrayCallback.h"
#include "minifi-cpp/controllers/SSLContextService.h"
#include "core/Deprecated.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::http {

struct HTTPProxy {
  std::string host;
  std::string username;
  std::string password;
  int port = 0;
};

class HTTPUploadCallback {
 public:
  virtual ~HTTPUploadCallback() = default;

  virtual size_t getDataChunk(char* data, size_t size) = 0;
  virtual size_t setPosition(int64_t offset) = 0;

  virtual size_t size() = 0;
  virtual void requestStop() = 0;
  virtual void close() = 0;
};

class HTTPUploadByteArrayInputCallback : public HTTPUploadCallback, public utils::ByteInputCallback {
 public:
  using ByteInputCallback::ByteInputCallback;

  size_t getDataChunk(char* data, size_t size) override;
  size_t setPosition(int64_t offset) override;

  size_t size() override { return getBufferSize(); }
  void requestStop() override { stop = true; }
  void close() override { utils::ByteInputCallback::close(); }

  std::atomic<bool> stop = false;
  std::atomic<size_t> pos = 0;
};

class HTTPUploadStreamContentsCallback : public HTTPUploadCallback {
 public:
  explicit HTTPUploadStreamContentsCallback(std::shared_ptr<io::InputStream> input_stream) : input_stream_{std::move(input_stream)} {}

  size_t getDataChunk(char* data, size_t size) override;
  size_t setPosition(int64_t offset) override;

 private:
  size_t size() override { return input_stream_->size(); }
  void requestStop() override {}
  void close() override {}

  std::shared_ptr<io::InputStream> input_stream_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HTTPUploadStreamContentsCallback>::getLogger();
};

class HTTPReadCallback : public utils::ByteOutputCallback {
 public:
  using ByteOutputCallback::ByteOutputCallback;

  std::atomic<bool> stop = false;
  std::atomic<size_t> pos = 0;
};

enum class SSLVersion : uint8_t {
  TLSv1_0,
  TLSv1_1,
  TLSv1_2,
  TLSv1_3
};

struct HTTPHeaderResponse {
 public:
  HTTPHeaderResponse() = default;

  static size_t receive_headers(void *buffer, size_t size, size_t nmemb, void *userp) {
    auto *pHeaders = static_cast<HTTPHeaderResponse*>(userp);
    if (pHeaders == nullptr) {
      return 0U;
    }
    pHeaders->header_tokens_.emplace_back(static_cast<char*>(buffer), size * nmemb);
    return size * nmemb;
  }

  [[nodiscard]] const std::vector<std::string>& getHeaderLines() const {
    return header_tokens_;
  }

  void clear() {
    parsed = false;
    header_tokens_.clear();
    header_mapping_.clear();
  }

  const std::map<std::string, std::string>& getHeaderMap() {
    if (!parsed) {
      std::string last_key;
      bool got_status_line = false;
      for (const auto& header_line : header_tokens_) {
        if (header_line.empty()) {
          /* This should not happen */
          continue;
        }
        if (!got_status_line) {
          if (header_line.compare(0, 4, "HTTP") == 0) {
            /* We got a status line now */
            got_status_line = true;
            header_mapping_.clear();
          }
          /* This is probably a chunked encoding trailer */
          continue;
        }
        if (header_line == "\r\n") {
          /* This is the end of the header */
          got_status_line = false;
          continue;
        }
        size_t separator_pos = header_line.find(':');
        if (separator_pos == std::string::npos) {
          if (!last_key.empty() && (header_line[0] == ' ' || header_line[0] == '\t')) {
            // This is a "folded header", which is deprecated (https://www.ietf.org/rfc/rfc7230.txt) but here we are
            header_mapping_[last_key].append(" " + utils::string::trim(header_line));
          }
          continue;
        }
        auto key = header_line.substr(0, separator_pos);
        /* This will remove leading and trailing LWS and the ending CRLF from the value */
        auto value = utils::string::trim(header_line.substr(separator_pos + 1));
        header_mapping_[key] = value;
        last_key = key;
      }
      parsed = true;
    }
    return header_mapping_;
  }

 private:
  std::vector<std::string> header_tokens_;
  std::map<std::string, std::string> header_mapping_;
  bool parsed{false};
};

namespace HTTPRequestResponse {
  const size_t CALLBACK_ABORT = 0x10000000;
  const int SEEKFUNC_OK = 0;
  const int SEEKFUNC_FAIL = 1;

  size_t receiveWrite(char * data, size_t size, size_t nmemb, void * p);
  size_t send_write(char * data, size_t size, size_t nmemb, void * p);
  int seek_callback(void *p, int64_t offset, int);
}

#undef DELETE  // this is a macro in winnt.h

enum class HttpRequestMethod {
  GET, POST, PUT, PATCH, DELETE, CONNECT, HEAD, OPTIONS, TRACE
};

class BaseHTTPClient {
 public:
  BaseHTTPClient() = default;

  virtual ~BaseHTTPClient() = default;

  virtual void setVerbose(bool use_stderr) = 0;

  virtual void initialize(HttpRequestMethod method, std::string url, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) = 0;

  virtual void setConnectionTimeout(std::chrono::milliseconds timeout) = 0;

  virtual void setReadTimeout(std::chrono::milliseconds timeout) = 0;

  virtual void setUploadCallback(std::unique_ptr<HTTPUploadCallback> callbackObj) = 0;

  virtual void setContentType(std::string content_type) = 0;

  virtual std::string escape(std::string string_to_escape) = 0;

  virtual void setPostFields(const std::string& input) = 0;

  virtual bool submit() = 0;

  [[nodiscard]] virtual int64_t getResponseCode() const = 0;

  virtual const char *getContentType() = 0;

  virtual void setRequestHeader(std::string key, std::optional<std::string> value) = 0;

  virtual void set_request_method(HttpRequestMethod method) = 0;

  virtual void setPeerVerification(bool peer_verification) = 0;
  virtual void setHostVerification(bool host_verification) = 0;

  virtual void setHTTPProxy(const HTTPProxy &proxy) = 0;

  virtual void setBasicAuth(const std::string& username, const std::string& password) = 0;
  virtual void clearBasicAuth() = 0;

  virtual bool setSpecificSSLVersion(SSLVersion specific_version) = 0;
  virtual bool setMinimumSSLVersion(SSLVersion minimum_version) = 0;

  virtual const std::vector<char>& getResponseBody() = 0;
  virtual const std::vector<std::string>& getResponseHeaders() = 0;
  virtual const std::map<std::string, std::string>& getResponseHeaderMap() = 0;
};

std::string get_token(BaseHTTPClient *client, const std::string& username, const std::string& password);

class URL {
 public:
  explicit URL(const std::string& url_input);
  [[nodiscard]] bool isValid() const { return is_valid_; }
  [[nodiscard]] std::string protocol() const { return protocol_; }
  [[nodiscard]] std::string host() const { return host_; }
  [[nodiscard]] int port() const;
  [[nodiscard]] std::string hostPort() const;
  [[nodiscard]] std::string toString() const;

 private:
  std::string protocol_;
  std::string host_;
  std::optional<int> port_;
  std::optional<std::string> path_;
  bool is_valid_ = false;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<URL>::getLogger();
};

}  // namespace org::apache::nifi::minifi::http
