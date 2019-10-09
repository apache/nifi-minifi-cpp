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
#include "HTTPClient.h"
#include "Exception.h"
#include <memory>
#include <climits>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include "utils/StringUtils.h"
#include "utils/RegexUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

HTTPClient::HTTPClient(const std::string &url, const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service)
    : core::Connectable("HTTPClient"),
      ssl_context_service_(ssl_context_service),
      url_(url),
      connect_timeout_(0),
      read_timeout_(0),
      content_type_str_(nullptr),
      headers_(nullptr),
      callback(nullptr),
      write_callback_(nullptr),
      http_code(0),
      read_callback_(INT_MAX),
      header_response_(-1),
      res(CURLE_OK),
      keep_alive_probe_(-1),
      keep_alive_idle_(-1),
      logger_(logging::LoggerFactory<HTTPClient>::getLogger()) {
  http_session_ = curl_easy_init();
}

HTTPClient::HTTPClient(std::string name, utils::Identifier uuid)
    : core::Connectable(name, uuid),
      ssl_context_service_(nullptr),
      url_(),
      connect_timeout_(0),
      read_timeout_(0),
      content_type_str_(nullptr),
      headers_(nullptr),
      callback(nullptr),
      write_callback_(nullptr),
      http_code(0),
      read_callback_(INT_MAX),
      header_response_(-1),
      res(CURLE_OK),
      keep_alive_probe_(-1),
      keep_alive_idle_(-1),
      logger_(logging::LoggerFactory<HTTPClient>::getLogger()) {
  http_session_ = curl_easy_init();
}

HTTPClient::HTTPClient()
    : core::Connectable("HTTPClient"),
      ssl_context_service_(nullptr),
      url_(),
      connect_timeout_(0),
      read_timeout_(0),
      content_type_str_(nullptr),
      headers_(nullptr),
      callback(nullptr),
      write_callback_(nullptr),
      http_code(0),
      read_callback_(INT_MAX),
      header_response_(-1),
      res(CURLE_OK),
      keep_alive_probe_(-1),
      keep_alive_idle_(-1),
      logger_(logging::LoggerFactory<HTTPClient>::getLogger()) {
  http_session_ = curl_easy_init();
}

HTTPClient::~HTTPClient() {
  if (nullptr != headers_) {
    curl_slist_free_all(headers_);
    headers_ = nullptr;
  }
  if (http_session_ != nullptr) {
    curl_easy_cleanup(http_session_);
    http_session_ = nullptr;
  }
  // forceClose ended up not being the issue in MINIFICPP-667, but leaving here
  // out of good hygiene.
  forceClose();
  read_callback_.close();
  logger_->log_trace("Closing HTTPClient for %s", url_);
}

void HTTPClient::forceClose() {

  if (nullptr != callback) {
    callback->stop = true;
  }

  if (nullptr != write_callback_) {
    write_callback_->stop = true;
  }

}

int HTTPClient::debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
  std::shared_ptr<logging::Logger>* logger = static_cast<std::shared_ptr<logging::Logger>*>(userptr);
  if (logger == nullptr) {
    return 0;
  }
  if (type == CURLINFO_TEXT) {
    logging::LOG_DEBUG((*logger)) << "CURL(" << reinterpret_cast<void*>(handle) << "): " << std::string(data, size);
  }
  return 0;
}

void HTTPClient::setVerbose(bool use_stderr /*= false*/) {
  curl_easy_setopt(http_session_, CURLOPT_VERBOSE, 1L);
  if (!use_stderr) {
    curl_easy_setopt(http_session_, CURLOPT_DEBUGDATA, &logger_);
    curl_easy_setopt(http_session_, CURLOPT_DEBUGFUNCTION, &debug_callback);
  }
}

void HTTPClient::initialize(const std::string &method, const std::string url, const std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) {
  method_ = method;
  set_request_method(method_);
  if (ssl_context_service != nullptr) {
    ssl_context_service_ = ssl_context_service;
  }
  if (!url.empty()) {
    url_ = url;
  }
  if (isSecure(url_) && ssl_context_service_ != nullptr) {
    configure_secure_connection(http_session_);
  }
}

void HTTPClient::setDisablePeerVerification() {
  logger_->log_debug("Disabling peer verification");
  curl_easy_setopt(http_session_, CURLOPT_SSL_VERIFYPEER, 0L);
}

void HTTPClient::setDisableHostVerification() {
  logger_->log_debug("Disabling host verification");
  curl_easy_setopt(http_session_, CURLOPT_SSL_VERIFYHOST, 0L);
}

bool HTTPClient::setSpecificSSLVersion(SSLVersion specific_version) {
#if CURL_AT_LEAST_VERSION(7, 54, 0)
  CURLcode ret = CURLE_UNKNOWN_OPTION;
  switch (specific_version) {
    case SSLVersion::TLSv1_0:
      ret = curl_easy_setopt(http_session_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_0 | CURL_SSLVERSION_MAX_TLSv1_0);
      break;
    case SSLVersion::TLSv1_1:
      ret = curl_easy_setopt(http_session_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_1 | CURL_SSLVERSION_MAX_TLSv1_1);
      break;
    case SSLVersion::TLSv1_2:
      ret = curl_easy_setopt(http_session_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2);
      break;
  }

  return ret == CURLE_OK;
#else
  return false;
#endif
}

/* If not set, the default will be TLS 1.0, see https://curl.haxx.se/libcurl/c/CURLOPT_SSLVERSION.html */
bool HTTPClient::setMinimumSSLVersion(SSLVersion minimum_version) {
  CURLcode ret = CURLE_UNKNOWN_OPTION;
  switch (minimum_version) {
    case SSLVersion::TLSv1_0:
      ret = curl_easy_setopt(http_session_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_0);
      break;
    case SSLVersion::TLSv1_1:
      ret = curl_easy_setopt(http_session_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_1);
      break;
    case SSLVersion::TLSv1_2:
      ret = curl_easy_setopt(http_session_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
      break;
  }

  return ret == CURLE_OK;
}

void HTTPClient::setConnectionTimeout(int64_t timeout) {
  connect_timeout_ = timeout;
  curl_easy_setopt(http_session_, CURLOPT_NOSIGNAL, 1);
}

void HTTPClient::setReadTimeout(int64_t timeout) {
  read_timeout_ = timeout;
}

void HTTPClient::setReadCallback(HTTPReadCallback *callbackObj) {
  callback = callbackObj;
  curl_easy_setopt(http_session_, CURLOPT_WRITEFUNCTION, &utils::HTTPRequestResponse::recieve_write);
  curl_easy_setopt(http_session_, CURLOPT_WRITEDATA, static_cast<void*>(callbackObj));
}

void HTTPClient::setUploadCallback(HTTPUploadCallback *callbackObj) {
  logger_->log_debug("Setting callback for %s", url_);
  write_callback_ = callbackObj;
  if (method_ == "put" || method_ == "PUT") {
    curl_easy_setopt(http_session_, CURLOPT_INFILESIZE_LARGE, (curl_off_t ) callbackObj->ptr->getBufferSize());
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

void HTTPClient::setPostFields(const std::string& input) {
  curl_easy_setopt(http_session_, CURLOPT_POSTFIELDSIZE, input.length());
  curl_easy_setopt(http_session_, CURLOPT_COPYPOSTFIELDS, input.c_str());
}

void HTTPClient::setPostSize(size_t size) {
  curl_easy_setopt(http_session_, CURLOPT_POSTFIELDSIZE, size);
}

void HTTPClient::setHeaders(struct curl_slist *list) {
  headers_ = list;
}

void HTTPClient::appendHeader(const std::string &new_header) {
  headers_ = curl_slist_append(headers_, new_header.c_str());
}

void HTTPClient::appendHeader(const std::string &key, const std::string &value) {
  std::stringstream new_header;
  new_header << key << ": " << value;
  headers_ = curl_slist_append(headers_, new_header.str().c_str());
}

void HTTPClient::setUseChunkedEncoding() {
  headers_ = curl_slist_append(headers_, "Transfer-Encoding: chunked");
}

bool HTTPClient::submit() {
  if (IsNullOrEmpty(url_))
    return false;
  if (connect_timeout_ > 0) {
    curl_easy_setopt(http_session_, CURLOPT_CONNECTTIMEOUT, connect_timeout_);
  }

  if (headers_ != nullptr) {
    headers_ = curl_slist_append(headers_, "Expect:");
    curl_easy_setopt(http_session_, CURLOPT_HTTPHEADER, headers_);
  }

  curl_easy_setopt(http_session_, CURLOPT_URL, url_.c_str());
  logger_->log_debug("Submitting to %s", url_);
  if (callback == nullptr) {
    content_.ptr = &read_callback_;
    curl_easy_setopt(http_session_, CURLOPT_WRITEFUNCTION, &utils::HTTPRequestResponse::recieve_write);
    curl_easy_setopt(http_session_, CURLOPT_WRITEDATA, static_cast<void*>(&content_));
  }
  curl_easy_setopt(http_session_, CURLOPT_HEADERFUNCTION, &utils::HTTPHeaderResponse::receive_headers);
  curl_easy_setopt(http_session_, CURLOPT_HEADERDATA, static_cast<void*>(&header_response_));
  if (keep_alive_probe_ > 0){
    logger_->log_debug("Setting keep alive to %d",keep_alive_probe_);
    curl_easy_setopt(http_session_, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(http_session_, CURLOPT_TCP_KEEPINTVL, keep_alive_probe_);
    curl_easy_setopt(http_session_, CURLOPT_TCP_KEEPIDLE, keep_alive_idle_);

  }
  else{
    logger_->log_debug("Not using keep alive");
    curl_easy_setopt(http_session_, CURLOPT_TCP_KEEPALIVE, 0L);
  }
  res = curl_easy_perform(http_session_);
  if (callback == nullptr) {
    read_callback_.close();
  }
  curl_easy_getinfo(http_session_, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_getinfo(http_session_, CURLINFO_CONTENT_TYPE, &content_type_str_);
  if (res != CURLE_OK) {
    logger_->log_error("curl_easy_perform() failed %s on %s\n", curl_easy_strerror(res), url_);
    return false;
  }

  logger_->log_debug("Finished with %s", url_);
  return true;
}

CURLcode HTTPClient::getResponseResult() {
  return res;
}

int64_t &HTTPClient::getResponseCode() {
  return http_code;
}

const char *HTTPClient::getContentType() {
  return content_type_str_;
}

const std::vector<char> &HTTPClient::getResponseBody() {
  if (response_body_.size() == 0) {
    if (callback && callback->ptr) {
      response_body_ = callback->ptr->to_string();
    } else {
      response_body_ = read_callback_.to_string();
    }
  }
  return response_body_;
}

void HTTPClient::set_request_method(const std::string method) {
  std::string my_method = method;
  std::transform(my_method.begin(), my_method.end(), my_method.begin(), ::toupper);
  if (my_method == "POST") {
    curl_easy_setopt(http_session_, CURLOPT_POST, 1L);
  } else if (my_method == "PUT") {
    curl_easy_setopt(http_session_, CURLOPT_UPLOAD, 1L);
  } else if (my_method == "HEAD") {
    curl_easy_setopt(http_session_, CURLOPT_NOBODY, 1L);
  } else if (my_method == "GET") {
  } else {
    curl_easy_setopt(http_session_, CURLOPT_CUSTOMREQUEST, my_method.c_str());
  }
}

bool HTTPClient::matches(const std::string &value, const std::string &sregex) {
  if (sregex == ".*")
    return true;
  try {
    utils::Regex rgx(sregex);
    return rgx.match(value);
  } catch (const Exception &e) {
    return false;
  }
}

void HTTPClient::configure_secure_connection(CURL *http_session) {
  logger_->log_debug("Using certificate file \"%s\"", ssl_context_service_->getCertificateFile());
  logger_->log_debug("Using private key file \"%s\"", ssl_context_service_->getPrivateKeyFile());
  logger_->log_debug("Using CA certificate file \"%s\"", ssl_context_service_->getCACertificate());
#if 0 // Reenable this path once we change from the direct manipulation of the SSL context to using the cURL API
  if (!ssl_context_service_->getCertificateFile().empty()) {
    if (utils::StringUtils::endsWithIgnoreCase(ssl_context_service_->getCertificateFile(),"p12")) {
      curl_easy_setopt(http_session, CURLOPT_SSLCERTTYPE, "P12");
    }
    else {
      curl_easy_setopt(http_session, CURLOPT_SSLCERTTYPE, "PEM");
    }
    curl_easy_setopt(http_session, CURLOPT_SSLCERT, ssl_context_service_->getCertificateFile().c_str());
  }
  if (!ssl_context_service_->getPrivateKeyFile().empty()) {
    if (utils::StringUtils::endsWithIgnoreCase(ssl_context_service_->getPrivateKeyFile(),"p12")) {
      curl_easy_setopt(http_session, CURLOPT_SSLKEYTYPE, "P12");
    }
    else {
      curl_easy_setopt(http_session, CURLOPT_SSLKEYTYPE, "PEM");
    }
    curl_easy_setopt(http_session, CURLOPT_SSLKEY, ssl_context_service_->getPrivateKeyFile().c_str());
    curl_easy_setopt(http_session, CURLOPT_KEYPASSWD, ssl_context_service_->getPassphrase().c_str());
  }
  if (!ssl_context_service_->getCACertificate().empty()) {
    curl_easy_setopt(http_session, CURLOPT_CAINFO, ssl_context_service_->getCACertificate().c_str());
  } else {
    curl_easy_setopt(http_session, CURLOPT_CAINFO, nullptr);
  }
  curl_easy_setopt(http_session, CURLOPT_CAPATH, nullptr);
#else
#ifdef OPENSSL_SUPPORT
  curl_easy_setopt(http_session, CURLOPT_SSL_CTX_FUNCTION, &configure_ssl_context);
  curl_easy_setopt(http_session, CURLOPT_SSL_CTX_DATA, static_cast<void*>(ssl_context_service_.get()));
  curl_easy_setopt(http_session, CURLOPT_CAINFO, 0);
  curl_easy_setopt(http_session, CURLOPT_CAPATH, 0);
#endif
#endif
}

bool HTTPClient::isSecure(const std::string &url) {
  if (url.find("https") == 0U) {
    logger_->log_debug("%s is a secure url", url);
    return true;
  }
  return false;
}

void HTTPClient::setInterface(const std::string &ifc) {
  curl_easy_setopt(http_session_, CURLOPT_INTERFACE, ifc.c_str());
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
