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

#ifndef LIBMINIFI_INCLUDE_UTILS_HTTPCLIENT_H_
#define LIBMINIFI_INCLUDE_UTILS_HTTPCLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ByteArrayCallback.h"
#include "controllers/SSLContextService.h"
#include "core/Deprecated.h"
#include "utils/gsl.h"
#include "utils/OptionalUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

struct HTTPProxy {
  std::string host;
  std::string username;
  std::string password;
  int port = 0;
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

enum class SSLVersion : uint8_t {
  TLSv1_0,
  TLSv1_1,
  TLSv1_2,
};

struct HTTPHeaderResponse {
 public:
  HTTPHeaderResponse(int max) : max_tokens_(max) , parsed(false) {} // NOLINT

  /* Deprecated, headers are stored internally and can be accessed by getHeaderLines or getHeaderMap */
  DEPRECATED(/*deprecated in*/ 0.7.0, /*will remove in */ 2.0) void append(const std::string &header) {
    if (max_tokens_ == -1 || (int32_t)header_tokens_.size() <= max_tokens_) {
      header_tokens_.push_back(header);
    }
  }

  /* Deprecated, headers are stored internally and can be accessed by getHeaderLines or getHeaderMap */
  DEPRECATED(/*deprecated in*/ 0.7.0, /*will remove in */ 2.0) void append(const std::string &key, const std::string &value) {
    header_mapping_[key].append(value);
  }

  int32_t max_tokens_;
  std::vector<std::string> header_tokens_;
  std::map<std::string, std::string> header_mapping_;
  bool parsed;

  static size_t receive_headers(void *buffer, size_t size, size_t nmemb, void *userp) {
    HTTPHeaderResponse *pHeaders = static_cast<HTTPHeaderResponse*>(userp);
    if (pHeaders == nullptr) {
      return 0U;
    }
    pHeaders->header_tokens_.emplace_back(static_cast<char*>(buffer), size * nmemb);
    return size * nmemb;
  }

  const std::vector<std::string>& getHeaderLines() const {
    return header_tokens_;
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
            header_mapping_[last_key].append(" " + utils::StringUtils::trim(header_line));
          }
          continue;
        }
        auto key = header_line.substr(0, separator_pos);
        /* This will remove leading and trailing LWS and the ending CRLF from the value */
        auto value = utils::StringUtils::trim(header_line.substr(separator_pos + 1));
        header_mapping_[key] = value;
        last_key = key;
      }
      parsed = true;
    }
    return header_mapping_;
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
  static const size_t CALLBACK_ABORT = 0x10000000;
  static const int SEEKFUNC_OK = 0;
  static const int SEEKFUNC_FAIL = 1;

  const std::vector<char> &getData() {
    return data;
  }

  HTTPRequestResponse(const HTTPRequestResponse &other)
      : max_queue(other.max_queue) {
  }

  HTTPRequestResponse(size_t max) // NOLINT
      : max_queue(max) {
  }
  /**
   * Receive HTTP Response.
   */
  static size_t recieve_write(char * data, size_t size, size_t nmemb, void * p) {
    try {
      if (p == nullptr) {
        return CALLBACK_ABORT;
      }
      HTTPReadCallback *callback = static_cast<HTTPReadCallback *>(p);
      if (callback->stop) {
        return CALLBACK_ABORT;
      }
      callback->ptr->write(data, (size * nmemb));
      return (size * nmemb);
    } catch (...) {
      return CALLBACK_ABORT;
    }
  }

  /**
   * Callback for post, put, and patch operations
   * @param buffer
   * @param size size of buffer
   * @param nitems items to add
   * @param insteam input stream object.
   */

  static size_t send_write(char * data, size_t size, size_t nmemb, void * p) {
    try {
      if (p == nullptr) {
        return CALLBACK_ABORT;
      }
      HTTPUploadCallback *callback = reinterpret_cast<HTTPUploadCallback*>(p);
      if (callback->stop) {
        return CALLBACK_ABORT;
      }
      size_t buffer_size = callback->ptr->getBufferSize();
      if (callback->getPos() <= buffer_size) {
        size_t len = buffer_size - callback->pos;
        if (len <= 0) {
          return 0;
        }
        char *ptr = callback->ptr->getBuffer(callback->getPos());

        if (ptr == nullptr) {
          return 0;
        }
        if (len > size * nmemb)
          len = size * nmemb;
        memcpy(data, ptr, len);
        callback->pos += len;
        callback->ptr->seek(callback->getPos());
        return len;
      }
      return 0;
    } catch (...) {
      return CALLBACK_ABORT;
    }
  }

  static int seek_callback(void *p, int64_t offset, int) {
    try {
      if (p == nullptr) {
        return SEEKFUNC_FAIL;
      }
      HTTPUploadCallback *callback = reinterpret_cast<HTTPUploadCallback*>(p);
      if (callback->stop) {
        return SEEKFUNC_FAIL;
      }
      if (callback->ptr->getBufferSize() <= static_cast<size_t>(offset)) {
        return SEEKFUNC_FAIL;
      }
      callback->pos = offset;
      callback->ptr->seek(callback->getPos());
      return SEEKFUNC_OK;
    } catch (...) {
      return SEEKFUNC_FAIL;
    }
  }

  int read_data(uint8_t *buf, size_t size) {
    size_t size_to_read = size;
    if (size_to_read > data.size()) {
      size_to_read = data.size();
    }
    memcpy(buf, data.data(), size_to_read);
    return gsl::narrow<int>(size_to_read);
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
  BaseHTTPClient() = default;

  virtual ~BaseHTTPClient() = default;

  virtual void setVerbose(bool use_stderr = false) = 0;

  virtual void initialize(const std::string &method, const std::string url = "", const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service = nullptr) = 0;

  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) virtual void setConnectionTimeout(int64_t timeout) = 0;

  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) virtual void setReadTimeout(int64_t timeout) = 0;

  virtual void setConnectionTimeout(std::chrono::milliseconds timeout) = 0;

  virtual void setReadTimeout(std::chrono::milliseconds timeout) = 0;

  virtual void setUploadCallback(HTTPUploadCallback *callbackObj) = 0;

  virtual void setSeekFunction(HTTPUploadCallback *callbackObj) = 0;

  virtual void setContentType(std::string content_type) = 0;

  virtual std::string escape(std::string string_to_escape) = 0;

  virtual void setPostFields(const std::string& input) = 0;

  virtual bool submit() = 0;

  virtual int64_t getResponseCode() const = 0;

  virtual const char *getContentType() = 0;

  virtual const std::vector<char> &getResponseBody() {
    return response_body_;
  }

  virtual void appendHeader(const std::string &new_header) = 0;

  virtual void set_request_method(const std::string method) = 0;

  virtual void setUseChunkedEncoding() = 0;

  virtual void setDisablePeerVerification() = 0;

  virtual void setHTTPProxy(const utils::HTTPProxy &proxy) = 0;

  virtual void setDisableHostVerification() = 0;

  virtual bool setSpecificSSLVersion(SSLVersion specific_version) = 0;

  virtual bool setMinimumSSLVersion(SSLVersion minimum_version) = 0;

  virtual const std::vector<std::string> &getHeaders() {
    return headers_;
  }

  virtual const std::map<std::string, std::string> &getParsedHeaders() {
    return header_mapping_;
  }

 protected:
  std::vector<char> response_body_;
  std::vector<std::string> headers_;
  std::map<std::string, std::string> header_mapping_;

  virtual inline bool matches(const std::string &value, const std::string &sregex) = 0;
};

std::string get_token(utils::BaseHTTPClient *client, std::string username, std::string password);

class URL {
 public:
  explicit URL(const std::string& url_input);
  bool isValid() const { return is_valid_; }
  std::string protocol() const { return protocol_; }
  std::string host() const { return host_; }
  int port() const;
  std::string hostPort() const;
  std::string toString() const;

 private:
  std::string protocol_;
  std::string host_;
  utils::optional<int> port_;
  utils::optional<std::string> path_;
  bool is_valid_ = false;
  std::shared_ptr<logging::Logger> logger_ = logging::LoggerFactory<URL>::getLogger();
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_HTTPCLIENT_H_

#ifdef WIN32
#pragma warning(pop)
#endif  // NOLINT
