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
#include "utils/HTTPClient.h"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

HTTPClient::HTTPClient(const std::string &url, const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service)
    : ssl_context_service_(ssl_context_service),
      url_(url),
      logger_(logging::LoggerFactory<HTTPClient>::getLogger()),
      connect_timeout_(0),
      read_timeout_(0),
      content_type(nullptr),
      headers_(nullptr),
      http_code(0),
      header_response_(1),
      res(CURLE_OK) {
  HTTPClientInitializer *initializer = HTTPClientInitializer::getInstance();
  http_session_ = curl_easy_init();
}

HTTPClient::~HTTPClient() {
  if (nullptr != headers_) {
    curl_slist_free_all(headers_);
  }
  curl_easy_cleanup(http_session_);
}

void HTTPClient::setVerbose() {
  curl_easy_setopt(http_session_, CURLOPT_VERBOSE, 1L);
}

void HTTPClient::initialize(const std::string &method) {
  method_ = method;
  set_request_method(method_);
  if (isSecure(url_) && ssl_context_service_ != nullptr) {
    configure_secure_connection(http_session_);
  }
}

void HTTPClient::setDisablePeerVerification() {
  logger_->log_info("Disabling peer verification");
  curl_easy_setopt(http_session_, CURLOPT_SSL_VERIFYPEER, 0L);
}

void HTTPClient::setConnectionTimeout(int64_t timeout) {
  connect_timeout_ = timeout;
}

void HTTPClient::setReadTimeout(int64_t timeout) {
  read_timeout_ = timeout;
}

void HTTPClient::setUploadCallback(HTTPUploadCallback *callbackObj) {
  logger_->log_info("Setting callback");
  if (method_ == "put" || method_ == "PUT") {
    curl_easy_setopt(http_session_, CURLOPT_INFILESIZE_LARGE, (curl_off_t) callbackObj->ptr->getBufferSize());
  } else {
    curl_easy_setopt(http_session_, CURLOPT_POSTFIELDSIZE, (curl_off_t) callbackObj->ptr->getBufferSize());
  }
  curl_easy_setopt(http_session_, CURLOPT_READFUNCTION, &utils::HTTPRequestResponse::send_write);
  curl_easy_setopt(http_session_, CURLOPT_READDATA, static_cast<void*>(callbackObj));
}

struct curl_slist *HTTPClient::build_header_list(std::string regex, const std::map<std::string, std::string> &attributes) {
  if (http_session_) {
    for (auto attribute : attributes) {
      if (matches(attribute.first, regex)) {
        std::string attr = attribute.first + ":" + attribute.second;
        headers_ = curl_slist_append(headers_, attr.c_str());
      }
    }
  }
  return headers_;
}

void HTTPClient::setContentType(std::string content_type) {
  content_type_ = "Content-Type: " + content_type;
  headers_ = curl_slist_append(headers_, content_type_.c_str());
}

std::string HTTPClient::escape(std::string string_to_escape) {
  return curl_easy_escape(http_session_, string_to_escape.c_str(), string_to_escape.length());
}

void HTTPClient::setPostFields(std::string input) {
  curl_easy_setopt(http_session_, CURLOPT_POSTFIELDSIZE, input.length());
  curl_easy_setopt(http_session_, CURLOPT_POSTFIELDS, input.c_str());
}

void HTTPClient::setHeaders(struct curl_slist *list) {
  headers_ = list;
}

void HTTPClient::setUseChunkedEncoding() {
  headers_ = curl_slist_append(headers_, "Transfer-Encoding: chunked");
}

bool HTTPClient::submit() {
  if (connect_timeout_ > 0) {
    curl_easy_setopt(http_session_, CURLOPT_CONNECTTIMEOUT, connect_timeout_);
  }

  if (headers_ != nullptr) {
    headers_ = curl_slist_append(headers_, "Expect:");
    curl_easy_setopt(http_session_, CURLOPT_HTTPHEADER, headers_);
  }

  curl_easy_setopt(http_session_, CURLOPT_URL, url_.c_str());
  logger_->log_info("Submitting to %s", url_);
  curl_easy_setopt(http_session_, CURLOPT_WRITEFUNCTION, &utils::HTTPRequestResponse::recieve_write);
  curl_easy_setopt(http_session_, CURLOPT_WRITEDATA, static_cast<void*>(&content_));

  curl_easy_setopt(http_session_, CURLOPT_HEADERFUNCTION, &utils::HTTPHeaderResponse::receive_headers);
  curl_easy_setopt(http_session_, CURLOPT_HEADERDATA, static_cast<void*>(&header_response_));

  res = curl_easy_perform(http_session_);
  curl_easy_getinfo(http_session_, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_getinfo(http_session_, CURLINFO_CONTENT_TYPE, &content_type);
  if (res != CURLE_OK) {
    logger_->log_error("curl_easy_perform() failed %s\n", curl_easy_strerror(res));
    return false;
  }
  return true;
}

CURLcode HTTPClient::getResponseResult() {
  return res;
}

int64_t &HTTPClient::getResponseCode() {
  return http_code;
}

const char *HTTPClient::getContentType() {
  return content_type;
}

const std::vector<char> &HTTPClient::getResponseBody() {
  return content_.data;
}

void HTTPClient::set_request_method(const std::string method) {
  std::string my_method = method;
  std::transform(my_method.begin(), my_method.end(), my_method.begin(), ::toupper);
  if (my_method == "POST") {
    curl_easy_setopt(http_session_, CURLOPT_POST, 1L);
  } else if (my_method == "PUT") {
    curl_easy_setopt(http_session_, CURLOPT_PUT, 0L);
  } else if (my_method == "GET") {
  } else {
    curl_easy_setopt(http_session_, CURLOPT_CUSTOMREQUEST, my_method.c_str());
  }
}

bool HTTPClient::matches(const std::string &value, const std::string &sregex) {
  if (sregex == ".*")
    return true;

  regex_t regex;
  int ret = regcomp(&regex, sregex.c_str(), 0);
  if (ret)
    return false;
  ret = regexec(&regex, value.c_str(), (size_t) 0, NULL, 0);
  regfree(&regex);
  if (ret)
    return false;

  return true;
}

void HTTPClient::configure_secure_connection(CURL *http_session) {
  logger_->log_debug("Using certificate file %s", ssl_context_service_->getCertificateFile());
  curl_easy_setopt(http_session, CURLOPT_SSL_CTX_FUNCTION, &configure_ssl_context);
  curl_easy_setopt(http_session, CURLOPT_SSL_CTX_DATA, static_cast<void*>(ssl_context_service_.get()));
}

bool HTTPClient::isSecure(const std::string &url) {
  if (url.find("https") != std::string::npos) {
    logger_->log_debug("%s is a secure url", url);
    return true;
  }
  return false;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
