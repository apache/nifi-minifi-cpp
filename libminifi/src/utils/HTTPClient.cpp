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
#include <algorithm>
#include <string>

#include "utils/HTTPClient.h"
#include "utils/StringUtils.h"

namespace {

constexpr const char* HTTP = "http://";
constexpr const char* HTTPS = "https://";

utils::optional<std::string> parseProtocol(const std::string& url_input) {
  if (utils::StringUtils::startsWith(url_input, HTTP)) {
    return HTTP;
  } else if (utils::StringUtils::startsWith(url_input, HTTPS)) {
    return HTTPS;
  } else {
    return {};
  }
}

utils::optional<int> parsePortNumber(const std::string& port_string) {
  try {
    size_t pos;
    int port = std::stoi(port_string, &pos);
    if (pos == port_string.size()) {
      return port;
    }
  } catch (const std::invalid_argument&) {
  } catch (const std::out_of_range&) {
  }

  return {};
}

}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

std::string get_token(utils::BaseHTTPClient *client, std::string username, std::string password) {
  if (nullptr == client) {
    return "";
  }
  std::string token;

  client->setContentType("application/x-www-form-urlencoded");

  client->set_request_method("POST");

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
    logger_->log_error("Unknown protocol in URL '%s'", url_input);
    return;
  }

  std::string::const_iterator current_pos = url_input.begin();
  std::advance(current_pos, protocol_.size());

  constexpr const char HOST_TERMINATORS[] = ":/?#";
  std::string::const_iterator end_of_host = std::find_first_of(current_pos, url_input.end(), std::begin(HOST_TERMINATORS), std::end(HOST_TERMINATORS));
  host_ = std::string{current_pos, end_of_host};
  if (host_.empty()) {
    logger_->log_error("No host found in URL '%s'", url_input);
    return;
  }
  current_pos = end_of_host;

  if (current_pos != url_input.end() && *current_pos == ':') {
    constexpr const char PORT_TERMINATORS[] = "/?#";
    ++current_pos;
    std::string::const_iterator end_of_port = std::find_first_of(current_pos, url_input.end(), std::begin(PORT_TERMINATORS), std::end(PORT_TERMINATORS));
    const auto port_number = parsePortNumber(std::string{current_pos, end_of_port});
    if (port_number) {
      port_ = *port_number;
    } else {
      logger_->log_error("Could not parse the port number in URL '%s'", url_input);
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

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
