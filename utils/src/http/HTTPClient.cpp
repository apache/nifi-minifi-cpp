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
#include "http/HTTPClient.h"

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/pkcs12.h>

#include "core/Resource.h"
#include "magic_enum.hpp"
#include "range/v3/algorithm/all_of.hpp"
#include "range/v3/action/transform.hpp"
#include "utils/gsl.h"
#include "utils/HTTPUtils.h"
#include "utils/Literals.h"
#include "utils/StringUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::http {

HTTPClient::HTTPClient(std::string url, std::shared_ptr<minifi::controllers::SSLContextService>  ssl_context_service)
    : core::ConnectableImpl("HTTPClient"),
      ssl_context_service_(std::move(ssl_context_service)),
      url_(std::move(url)) {
  http_session_.reset(curl_easy_init());
}

HTTPClient::HTTPClient(std::string_view name, const utils::Identifier& uuid)
    : core::ConnectableImpl(name, uuid) {
  http_session_.reset(curl_easy_init());
}

HTTPClient::HTTPClient()
    : core::ConnectableImpl("HTTPClient") {
  http_session_.reset(curl_easy_init());
}

void HTTPClient::addFormPart(const std::string& content_type, const std::string& name, std::unique_ptr<HTTPUploadCallback> form_callback, const std::optional<std::string>& filename) {
  if (!form_) {
    form_.reset(curl_mime_init(http_session_.get()));
  }
  form_callback_ = std::move(form_callback);
  curl_mimepart* part = curl_mime_addpart(form_.get());
  curl_mime_type(part, content_type.c_str());
  if (filename) {
    curl_mime_filename(part, filename->c_str());
  }
  curl_mime_name(part, name.c_str());
  curl_mime_data_cb(part, gsl::narrow<curl_off_t>(form_callback_->size()),
      &HTTPRequestResponse::send_write, nullptr, nullptr, static_cast<void*>(form_callback_.get()));
}

HTTPClient::~HTTPClient() {
  // forceClose ended up not being the issue in MINIFICPP-667, but leaving here
  // out of good hygiene.
  forceClose();
  content_.close();
  logger_->log_trace("Closing HTTPClient for {}", url_);
}

void HTTPClient::forceClose() {
  if (nullptr != read_callback_) {
    read_callback_->stop = true;
  }

  if (nullptr != write_callback_) {
    write_callback_->requestStop();
  }
}

int HTTPClient::debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
  auto* const logger = static_cast<std::shared_ptr<core::logging::Logger>*>(userptr);
  if (logger == nullptr) {
    return 0;
  }
  if (type == CURLINFO_TEXT) {
    (*logger)->log_debug("CURL({}): {:.{}}", handle, data, size);
  }
  return 0;
}

void HTTPClient::setVerbose(bool use_stderr) {
  curl_easy_setopt(http_session_.get(), CURLOPT_VERBOSE, 1L);
  if (!use_stderr) {
    curl_easy_setopt(http_session_.get(), CURLOPT_DEBUGDATA, &logger_);
    curl_easy_setopt(http_session_.get(), CURLOPT_DEBUGFUNCTION, &debug_callback);
  }
}

namespace {
bool isSecure(const std::string& url) {
  return url.starts_with("https");
}
}  // namespace

void HTTPClient::initialize(http::HttpRequestMethod method, std::string url, std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service) {
  set_request_method(method);
  if (ssl_context_service) {
    ssl_context_service_ = std::move(ssl_context_service);
  }
  if (!url.empty()) {
    url_ = std::move(url);
  }
  if (isSecure(url_))
    configure_secure_connection();
}

void HTTPClient::setPeerVerification(bool peer_verification) {
  if (peer_verification) {
    logger_->log_debug("Enabling peer verification");
  } else {
    logger_->log_warn("Disabling peer verification: the authenticity of https servers will not be verified!");
  }
  curl_easy_setopt(http_session_.get(), CURLOPT_SSL_VERIFYPEER, peer_verification);
}

void HTTPClient::setHostVerification(bool host_verification) {
  logger_->log_debug("{} host verification", host_verification ? "Enabling" : "Disabling");
  curl_easy_setopt(http_session_.get(), CURLOPT_SSL_VERIFYHOST, host_verification);
}

void HTTPClient::setBasicAuth(const std::string& username, const std::string& password) {
  curl_easy_setopt(http_session_.get(), CURLOPT_USERNAME, username.c_str());
  curl_easy_setopt(http_session_.get(), CURLOPT_PASSWORD, password.c_str());
}

void HTTPClient::clearBasicAuth() {
  curl_easy_setopt(http_session_.get(), CURLOPT_USERNAME, nullptr);
  curl_easy_setopt(http_session_.get(), CURLOPT_PASSWORD, nullptr);
}

bool HTTPClient::setSpecificSSLVersion(SSLVersion specific_version) {
  if (ssl_context_service_) {
    switch (specific_version) {
      case SSLVersion::TLSv1_0: {
        ssl_context_service_->setMinTlsVersion(TLS1_VERSION);
        ssl_context_service_->setMaxTlsVersion(TLS1_VERSION);
        break;
      }
      case SSLVersion::TLSv1_1: {
        ssl_context_service_->setMinTlsVersion(TLS1_1_VERSION);
        ssl_context_service_->setMaxTlsVersion(TLS1_1_VERSION);
        break;
      }
      case SSLVersion::TLSv1_2: {
        ssl_context_service_->setMinTlsVersion(TLS1_2_VERSION);
        ssl_context_service_->setMaxTlsVersion(TLS1_2_VERSION);
        break;
      }
      case SSLVersion::TLSv1_3: {
        ssl_context_service_->setMinTlsVersion(TLS1_3_VERSION);
        ssl_context_service_->setMaxTlsVersion(TLS1_3_VERSION);
        break;
      }
      default: break;
    }
  }

  // bitwise or of different enum types is deprecated in C++20, but the curl api explicitly supports ORing one of CURL_SSLVERSION and one of CURL_SSLVERSION_MAX
  switch (specific_version) {
    case SSLVersion::TLSv1_0:
      return CURLE_OK == curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, static_cast<int>(CURL_SSLVERSION_TLSv1_0) | static_cast<int>(CURL_SSLVERSION_MAX_TLSv1_0));
    case SSLVersion::TLSv1_1:
      return CURLE_OK == curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, static_cast<int>(CURL_SSLVERSION_TLSv1_1) | static_cast<int>(CURL_SSLVERSION_MAX_TLSv1_1));
    case SSLVersion::TLSv1_2:
      return CURLE_OK == curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, static_cast<int>(CURL_SSLVERSION_TLSv1_2) | static_cast<int>(CURL_SSLVERSION_MAX_TLSv1_2));
    case SSLVersion::TLSv1_3:
      return CURLE_OK == curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, static_cast<int>(CURL_SSLVERSION_TLSv1_3) | static_cast<int>(CURL_SSLVERSION_MAX_TLSv1_3));
    default: return false;
  }
}

// If not set, the default will be TLS 1.0, see https://curl.haxx.se/libcurl/c/CURLOPT_SSLVERSION.html
bool HTTPClient::setMinimumSSLVersion(SSLVersion minimum_version) {
  if (ssl_context_service_) {
    switch (minimum_version) {
      case SSLVersion::TLSv1_0: {
        ssl_context_service_->setMinTlsVersion(TLS1_VERSION);
        break;
      }
      case SSLVersion::TLSv1_1: {
        ssl_context_service_->setMinTlsVersion(TLS1_1_VERSION);
        break;
      }
      case SSLVersion::TLSv1_2: {
        ssl_context_service_->setMinTlsVersion(TLS1_2_VERSION);
        break;
      }
      case SSLVersion::TLSv1_3: {
        ssl_context_service_->setMinTlsVersion(TLS1_3_VERSION);
        break;
      }
      default: break;
    }
  }

  CURLcode ret = CURLE_UNKNOWN_OPTION;
  switch (minimum_version) {
    case SSLVersion::TLSv1_0:
      ret = curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_0);
      break;
    case SSLVersion::TLSv1_1:
      ret = curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_1);
      break;
    case SSLVersion::TLSv1_2:
      ret = curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
      break;
    case SSLVersion::TLSv1_3:
      ret = curl_easy_setopt(http_session_.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
      break;
  }

  return ret == CURLE_OK;
}

void HTTPClient::setKeepAliveProbe(std::optional<KeepAliveProbeData> probe_data) {
  if (probe_data) {
    curl_easy_setopt(http_session_.get(), CURLOPT_TCP_KEEPALIVE, true);
    curl_easy_setopt(http_session_.get(), CURLOPT_TCP_KEEPINTVL, probe_data->keep_alive_interval.count());
    curl_easy_setopt(http_session_.get(), CURLOPT_TCP_KEEPIDLE, probe_data->keep_alive_delay.count());
  } else {
    curl_easy_setopt(http_session_.get(), CURLOPT_TCP_KEEPALIVE, false);
  }
}

void HTTPClient::setConnectionTimeout(std::chrono::milliseconds timeout) {
  if (timeout < 0ms) {
    logger_->log_error("Invalid HTTP connection timeout {}", timeout);
    return;
  }
  connect_timeout_ = timeout;
}

void HTTPClient::setReadTimeout(std::chrono::milliseconds timeout) {
  if (timeout < 0ms) {
    logger_->log_error("Invalid HTTP read timeout {}", timeout);
    return;
  }
  read_timeout_ = timeout;
}

void HTTPClient::setReadCallback(std::unique_ptr<HTTPReadCallback> callback) {
  read_callback_ = std::move(callback);
  curl_easy_setopt(http_session_.get(), CURLOPT_WRITEFUNCTION, &HTTPRequestResponse::receiveWrite);
  curl_easy_setopt(http_session_.get(), CURLOPT_WRITEDATA, static_cast<void*>(read_callback_.get()));
}

void HTTPClient::setUploadCallback(std::unique_ptr<HTTPUploadCallback> callback) {
  logger_->log_debug("Setting callback for {}", url_);
  write_callback_ = std::move(callback);
  if (method_ == http::HttpRequestMethod::PUT) {
    curl_easy_setopt(http_session_.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t) write_callback_->size());
  }
  curl_easy_setopt(http_session_.get(), CURLOPT_READFUNCTION, &HTTPRequestResponse::send_write);
  curl_easy_setopt(http_session_.get(), CURLOPT_READDATA, static_cast<void*>(write_callback_.get()));
  curl_easy_setopt(http_session_.get(), CURLOPT_SEEKDATA, static_cast<void*>(write_callback_.get()));
  curl_easy_setopt(http_session_.get(), CURLOPT_SEEKFUNCTION, &HTTPRequestResponse::seek_callback);
}

void HTTPClient::setContentType(std::string content_type) {
  request_headers_["Content-Type"] = std::move(content_type);
}

std::string HTTPClient::escape(std::string string_to_escape) {
  struct curl_deleter { void operator()(void* p) noexcept { curl_free(p); } };
  std::unique_ptr<char, curl_deleter> escaped_chars{curl_easy_escape(http_session_.get(), string_to_escape.c_str(), gsl::narrow<int>(string_to_escape.length()))};
  std::string escaped_string(escaped_chars.get());
  return escaped_string;
}

void HTTPClient::setPostFields(const std::string& input) {
  setPostSize(input.length());
  curl_easy_setopt(http_session_.get(), CURLOPT_COPYPOSTFIELDS, input.c_str());
}

void HTTPClient::setPostSize(size_t size) {
  if (size > 2_GB) {
    curl_easy_setopt(http_session_.get(), CURLOPT_POSTFIELDSIZE_LARGE, size);
  } else {
    curl_easy_setopt(http_session_.get(), CURLOPT_POSTFIELDSIZE, size);
  }
}

void HTTPClient::setHTTPProxy(const HTTPProxy &proxy) {
  if (!proxy.host.empty()) {
    curl_easy_setopt(http_session_.get(), CURLOPT_PROXY, proxy.host.c_str());
    curl_easy_setopt(http_session_.get(), CURLOPT_PROXYPORT, proxy.port);
    if (!proxy.username.empty()) {
      curl_easy_setopt(http_session_.get(), CURLOPT_PROXYAUTH, CURLAUTH_ANY);
      std::string value = proxy.username + ":" + proxy.password;
      curl_easy_setopt(http_session_.get(), CURLOPT_PROXYUSERPWD, value.c_str());
    }
  }
}

void HTTPClient::setRequestHeader(std::string key, std::optional<std::string> value) {
  if (value)
    request_headers_[std::move(key)] = std::move(*value);
  else
    request_headers_.erase(key);
}

namespace {
using CurlSlistDeleter = decltype([](struct curl_slist* slist) { curl_slist_free_all(slist); });

std::unique_ptr<struct curl_slist, CurlSlistDeleter> toCurlSlist(const std::unordered_map<std::string, std::string>& request_headers) {
  gsl::owner<curl_slist*> new_list = nullptr;
  const auto guard = gsl::finally([&new_list]() { curl_slist_free_all(new_list); });
  for (const auto& [header_key, header_value] : request_headers)
    new_list = (utils::optional_from_ptr(curl_slist_append(new_list, utils::string::join_pack(header_key, ": ", header_value).c_str()))  // NOLINT(cppcoreguidelines-owning-memory)
        | utils::orElse([]() { throw std::runtime_error{"curl_slist_append failed"}; })).value();

  return {std::exchange(new_list, nullptr), {}};
}
}  // namespace


bool HTTPClient::submit() {
  if (url_.empty()) {
    logger_->log_error("Tried to submit to an empty url");
    return false;
  }
  if (!method_) {
    logger_->log_error("Tried to use HTTPClient without setting an HTTP method");
    return false;
  }

  response_data_.clear();

  curl_easy_setopt(http_session_.get(), CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(http_session_.get(), CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_.count());
  curl_easy_setopt(http_session_.get(), CURLOPT_TIMEOUT_MS, getAbsoluteTimeout().count());

  if (read_timeout_ > 0ms) {
    progress_.reset();
    curl_easy_setopt(http_session_.get(), CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(http_session_.get(), CURLOPT_XFERINFOFUNCTION, onProgress);
    curl_easy_setopt(http_session_.get(), CURLOPT_XFERINFODATA, this);
  } else {
    // the user explicitly set it to 0
    curl_easy_setopt(http_session_.get(), CURLOPT_NOPROGRESS, 1);
  }

  const auto headers = toCurlSlist(request_headers_);
  if (headers) {
    curl_slist_append(headers.get(), "Expect:");
    curl_easy_setopt(http_session_.get(), CURLOPT_HTTPHEADER, headers.get());
  }

  curl_easy_setopt(http_session_.get(), CURLOPT_URL, url_.c_str());
  logger_->log_debug("Submitting to {}", url_);
  if (read_callback_ == nullptr) {
    curl_easy_setopt(http_session_.get(), CURLOPT_WRITEFUNCTION, &HTTPRequestResponse::receiveWrite);
    curl_easy_setopt(http_session_.get(), CURLOPT_WRITEDATA, static_cast<void*>(&content_));
  }

  curl_easy_setopt(http_session_.get(), CURLOPT_HEADERFUNCTION, &HTTPHeaderResponse::receive_headers);
  curl_easy_setopt(http_session_.get(), CURLOPT_HEADERDATA, static_cast<void*>(&response_data_.header_response));

  if (form_ != nullptr) {
    curl_easy_setopt(http_session_.get(), CURLOPT_MIMEPOST, form_.get());
  }
  res_ = curl_easy_perform(http_session_.get());
  if (read_callback_ == nullptr) {
    content_.close();
  }
  long http_code = 0;  // NOLINT(runtime/int,google-runtime-int) long due to libcurl API
  curl_easy_getinfo(http_session_.get(), CURLINFO_RESPONSE_CODE, &http_code);
  response_data_.response_code = http_code;
  curl_easy_getinfo(http_session_.get(), CURLINFO_CONTENT_TYPE, &response_data_.response_content_type);
  if (res_ == CURLE_OPERATION_TIMEDOUT) {
    logger_->log_error("HTTP operation timed out, with absolute timeout {}\n", getAbsoluteTimeout());
  }
  if (res_ != CURLE_OK) {
    logger_->log_info("{}", request_headers_.size());
    logger_->log_error("curl_easy_perform() failed {} on {}, error code {}\n", curl_easy_strerror(res_), url_, magic_enum::enum_underlying(res_));
    return false;
  }

  logger_->log_debug("Finished with {}", url_);
  return true;
}

int64_t HTTPClient::getResponseCode() const {
  return response_data_.response_code;
}

const char *HTTPClient::getContentType() {
  return response_data_.response_content_type;
}

const std::vector<char> &HTTPClient::getResponseBody() {
  if (response_data_.response_body.empty()) {
    if (read_callback_) {
      response_data_.response_body = read_callback_->to_string();
    } else {
      response_data_.response_body = content_.to_string();
    }
  }
  return response_data_.response_body;
}

void HTTPClient::set_request_method(http::HttpRequestMethod method) {
  if (method_ == method)
    return;
  method_ = method;
  switch (*method_) {
    case http::HttpRequestMethod::POST:
      curl_easy_setopt(http_session_.get(), CURLOPT_POST, 1L);
      curl_easy_setopt(http_session_.get(), CURLOPT_CUSTOMREQUEST, nullptr);
      break;
    case http::HttpRequestMethod::HEAD:
      curl_easy_setopt(http_session_.get(), CURLOPT_NOBODY, 1L);
      curl_easy_setopt(http_session_.get(), CURLOPT_CUSTOMREQUEST, nullptr);
      break;
    case http::HttpRequestMethod::GET:
      curl_easy_setopt(http_session_.get(), CURLOPT_HTTPGET, 1L);
      curl_easy_setopt(http_session_.get(), CURLOPT_CUSTOMREQUEST, nullptr);
      break;
    case http::HttpRequestMethod::PUT:
      curl_easy_setopt(http_session_.get(), CURLOPT_UPLOAD, 1L);
      curl_easy_setopt(http_session_.get(), CURLOPT_CUSTOMREQUEST, nullptr);
      break;
    default:
      curl_easy_setopt(http_session_.get(), CURLOPT_POST, 0L);
      curl_easy_setopt(http_session_.get(), CURLOPT_NOBODY, 0L);
      curl_easy_setopt(http_session_.get(), CURLOPT_HTTPGET, 0L);
      curl_easy_setopt(http_session_.get(), CURLOPT_UPLOAD, 0L);
      curl_easy_setopt(http_session_.get(), CURLOPT_CUSTOMREQUEST, std::string(magic_enum::enum_name(*method_)).c_str());
      break;
  }
}

int HTTPClient::onProgress(void *clientp, curl_off_t /*dltotal*/, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t ulnow) {
  gsl_Expects(clientp);
  HTTPClient& client = *static_cast<HTTPClient*>(clientp);
  auto now = std::chrono::steady_clock::now();
  auto elapsed = now - client.progress_.last_transferred_;
  if (dlnow != client.progress_.downloaded_data_ || ulnow != client.progress_.uploaded_data_) {
    // did transfer data
    client.progress_.last_transferred_ = now;
    client.progress_.downloaded_data_ = dlnow;
    client.progress_.uploaded_data_ = ulnow;
    return 0;
  }
  // did not transfer data
  if (elapsed > client.read_timeout_) {
    // timeout
    client.logger_->log_error("HTTP operation has been idle for {}, limit ({}) reached, terminating connection\n", elapsed, client.read_timeout_);
    return 1;
  }
  return 0;
}

void HTTPClient::configure_secure_connection() {
  if (ssl_context_service_) {
    curl_easy_setopt(http_session_.get(), CURLOPT_SSL_CTX_FUNCTION, &configure_ssl_context);
    curl_easy_setopt(http_session_.get(), CURLOPT_SSL_CTX_DATA, static_cast<void *>(ssl_context_service_.get()));
    curl_easy_setopt(http_session_.get(), CURLOPT_CAINFO, nullptr);
    curl_easy_setopt(http_session_.get(), CURLOPT_CAPATH, nullptr);
  } else {
    static const auto default_ca_file = utils::getDefaultCAFile();

    if (default_ca_file)
      logger_->log_debug("Using CA certificate file \"{}\"", *default_ca_file);
    else
      logger_->log_error("Could not find valid CA certificate file");

    curl_easy_setopt(http_session_.get(), CURLOPT_SSL_CTX_FUNCTION, nullptr);
    curl_easy_setopt(http_session_.get(), CURLOPT_SSL_CTX_DATA, nullptr);
    if (default_ca_file)
      curl_easy_setopt(http_session_.get(), CURLOPT_CAINFO, std::string(*default_ca_file).c_str());
    else
      curl_easy_setopt(http_session_.get(), CURLOPT_CAINFO, nullptr);
    curl_easy_setopt(http_session_.get(), CURLOPT_CAPATH, nullptr);
  }
}

void HTTPClient::setInterface(const std::string &ifc) {
  curl_easy_setopt(http_session_.get(), CURLOPT_INTERFACE, ifc.c_str());
}

void HTTPClient::setFollowRedirects(bool follow) {
  curl_easy_setopt(http_session_.get(), CURLOPT_FOLLOWLOCATION, follow);
}

void HTTPClient::setMaximumUploadSpeed(uint64_t max_bytes_per_second) {
  curl_easy_setopt(http_session_.get(), CURLOPT_MAX_SEND_SPEED_LARGE, max_bytes_per_second);
}

void HTTPClient::setMaximumDownloadSpeed(uint64_t max_bytes_per_second) {
  curl_easy_setopt(http_session_.get(), CURLOPT_MAX_RECV_SPEED_LARGE, max_bytes_per_second);
}

bool HTTPClient::isValidHttpHeaderField(std::string_view field_name) {
  if (field_name.empty()) {
    return false;
  }

  // RFC822 3.1.2: The  field-name must be composed of printable ASCII characters
  // (i.e., characters that  have  values  between  33.  and  126., decimal, except colon).
  return ranges::all_of(field_name, [](char c) { return c >= 33 && c <= 126 && c != ':'; });
}

std::string HTTPClient::replaceInvalidCharactersInHttpHeaderFieldName(std::string field_name) {
  if (field_name.empty()) {
    return "X-MiNiFi-Empty-Attribute-Name";
  }

  // RFC822 3.1.2: The  field-name must be composed of printable ASCII characters
  // (i.e., characters that  have  values  between  33.  and  126., decimal, except colon).
  ranges::actions::transform(field_name, [](char ch) {  // NOLINT: false positive: Add #include <algorithm> for transform  [build/include_what_you_use] [4]
    return (ch >= 33 && ch <= 126 && ch != ':') ? ch : '-';
  });
  return field_name;
}

void HTTPClient::CurlEasyCleanup::operator()(CURL* curl) const {
  curl_easy_cleanup(curl);
}

void HTTPClient::CurlMimeFree::operator()(curl_mime* curl_mime) const {
  curl_mime_free(curl_mime);
}

REGISTER_RESOURCE(HTTPClient, InternalResource);

}  // namespace org::apache::nifi::minifi::http
