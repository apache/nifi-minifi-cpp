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
#include "http/BaseHTTPClient.h"

#include <algorithm>
#include <string>

#include "utils/StringUtils.h"

namespace minifi = org::apache::nifi::minifi;

namespace {

constexpr const char* HTTP = "http://";
constexpr const char* HTTPS = "https://";


std::optional<std::string> parseProtocol(const std::string& url_input) {
  if (minifi::utils::string::startsWith(url_input, HTTP)) {
    return HTTP;
  } else if (minifi::utils::string::startsWith(url_input, HTTPS)) {
    return HTTPS;
  } else {
    return std::nullopt;
  }
}

std::optional<int> parsePortNumber(const std::string& port_string) {
  try {
    size_t pos = 0;
    int port = std::stoi(port_string, &pos);
    if (pos == port_string.size()) {
      return port;
    }
  } catch (const std::invalid_argument&) {
  } catch (const std::out_of_range&) {
  }

  return std::nullopt;
}

}  // namespace

namespace org::apache::nifi::minifi::http {

std::string get_token(BaseHTTPClient* client, const std::string& username, const std::string& password) {
  if (nullptr == client) {
    return "";
  }
  std::string token;

  client->setContentType("application/x-www-form-urlencoded");

  client->set_request_method(HttpRequestMethod::POST);

  std::string payload = "username=" + username + "&" + "password=" + password;

  client->setPostFields(client->escape(payload));

  client->submit();

  if (client->submit() && client->getResponseCode() == 200) {
    const std::string &response_body = std::string(client->getResponseBody().data(), client->getResponseBody().size());
    if (!response_body.empty()) {
      token = "Bearer " + response_body;
    }
  }
  return token;
}

URL::URL(const std::string& url_input) {
  const auto protocol = parseProtocol(url_input);
  if (protocol) {
    protocol_ = *protocol;
  } else {
    logger_->log_error("Unknown protocol in URL '{}'", url_input);
    return;
  }

  std::string::const_iterator current_pos = url_input.begin();
  std::advance(current_pos, protocol_.size());

  static constexpr std::string_view HOST_TERMINATORS = ":/?#";
  std::string::const_iterator end_of_host = std::find_first_of(current_pos, url_input.end(), std::begin(HOST_TERMINATORS), std::end(HOST_TERMINATORS));
  host_ = std::string{current_pos, end_of_host};
  if (host_.empty()) {
    logger_->log_error("No host found in URL '{}'", url_input);
    return;
  }
  current_pos = end_of_host;

  if (current_pos != url_input.end() && *current_pos == ':') {
    static constexpr std::string_view PORT_TERMINATORS = "/?#";
    ++current_pos;
    std::string::const_iterator end_of_port = std::find_first_of(current_pos, url_input.end(), std::begin(PORT_TERMINATORS), std::end(PORT_TERMINATORS));
    const auto port_number = parsePortNumber(std::string{current_pos, end_of_port});
    if (port_number) {
      port_ = *port_number;
    } else {
      logger_->log_error("Could not parse the port number in URL '{}'", url_input);
      return;
    }
    current_pos = end_of_port;
  }

  if (current_pos != url_input.end()) {
    path_ = std::string{current_pos, url_input.end()};
  }

  is_valid_ = true;
}

int URL::port() const {
  if (port_) {
    return *port_;
  } else if (protocol_ == HTTP) {
    return 80;
  } else if (protocol_ == HTTPS) {
    return 443;
  } else {
    throw std::logic_error{"Undefined port in URL: " + toString()};
  }
}

std::string URL::hostPort() const {
  if (!isValid()) {
    return "INVALID";
  }

  if (port_) {
    return protocol_ + host_ + ':' + std::to_string(*port_);
  } else {
    return protocol_ + host_;
  }
}

std::string URL::toString() const {
  if (!isValid()) {
    return "INVALID";
  }

  if (path_) {
    return hostPort() + *path_;
  } else {
    return hostPort();
  }
}

/**
 * Receive HTTP Response.
 */
size_t HTTPRequestResponse::receiveWrite(char *data, size_t size, size_t nmemb, void *p) {
  try {
    if (p == nullptr) {
      return CALLBACK_ABORT;
    }
    auto *callback = static_cast<HTTPReadCallback *>(p);
    if (callback->stop) {
      return CALLBACK_ABORT;
    }
    callback->write(data, (size * nmemb));
    return (size * nmemb);
  } catch (...) {
    return CALLBACK_ABORT;
  }
}

/**
 * Callback for post, put, and patch operations
 * @param data output buffer to write to
 * @param size number of elements to write
 * @param nmemb size of each element to write
 * @param p input object to read from
 */
size_t HTTPRequestResponse::send_write(char *data, size_t size, size_t nmemb, void *p) {
  try {
    if (p == nullptr) {
      return CALLBACK_ABORT;
    }
    auto *callback = reinterpret_cast<HTTPUploadCallback *>(p);
    return callback->getDataChunk(data, size * nmemb);
  } catch (...) {
    return CALLBACK_ABORT;
  }
}

int HTTPRequestResponse::seek_callback(void *p, int64_t offset, int) {
  try {
    if (p == nullptr) {
      return SEEKFUNC_FAIL;
    }
    auto *callback = reinterpret_cast<HTTPUploadCallback *>(p);
    return gsl::narrow<int>(callback->setPosition(offset));
  } catch (...) {
    return SEEKFUNC_FAIL;
  }
}

size_t HTTPUploadByteArrayInputCallback::getDataChunk(char *data, size_t size) {
  if (stop) {
    return HTTPRequestResponse::CALLBACK_ABORT;
  }
  size_t buffer_size = getBufferSize();
  if (pos <= buffer_size) {
    size_t len = buffer_size - pos;
    if (len <= 0) {
      return 0;
    }
    auto *ptr = getBuffer(pos);

    if (ptr == nullptr) {
      return 0;
    }
    if (len > size)
      len = size;
    memcpy(data, ptr, len);
    pos += len;
    seek(pos);
    return len;
  }
  return 0;
}

size_t HTTPUploadByteArrayInputCallback::setPosition(int64_t offset) {
  if (stop) {
    return HTTPRequestResponse::SEEKFUNC_FAIL;
  }
  if (getBufferSize() <= static_cast<size_t>(offset)) {
    return HTTPRequestResponse::SEEKFUNC_FAIL;
  }
  pos = offset;
  seek(pos);
  return HTTPRequestResponse::SEEKFUNC_OK;
}

size_t HTTPUploadStreamContentsCallback::getDataChunk(char *data, size_t size) {
  logger_->log_trace("HTTPUploadStreamContentsCallback is asked for up to {} bytes", size);

  std::span<char> buffer{data, size};
  size_t num_read = input_stream_->read(as_writable_bytes(buffer));

  if (io::isError(num_read)) {
    logger_->log_error("Error reading the input stream in HTTPUploadStreamContentsCallback");
    return 0;
  }

  logger_->log_debug("HTTPUploadStreamContentsCallback is returning {} bytes", num_read);
  return num_read;
}

size_t HTTPUploadStreamContentsCallback::setPosition(int64_t offset) {
  if (offset == 0) {
    logger_->log_debug("HTTPUploadStreamContentsCallback is ignoring request to rewind to the beginning");
  } else {
    logger_->log_warn("HTTPUploadStreamContentsCallback is ignoring request to seek to position {}", offset);
  }

  return HTTPRequestResponse::SEEKFUNC_OK;
}

}  // namespace org::apache::nifi::minifi::http
