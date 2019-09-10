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
#include <string>
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

void parse_url(const std::string *url, std::string *host, int *port, std::string *protocol) {
  static std::string http("http://");
  static std::string https("https://");

  if (url->compare(0, http.size(), http) == 0)
    *protocol = http;

  if (url->compare(0, https.size(), https) == 0)
    *protocol = https;

  if (!protocol->empty()) {
    size_t pos = url->find_first_of(":", protocol->size());

    if (pos == std::string::npos) {
      pos = url->size();
    }

    *host = url->substr(protocol->size(), pos - protocol->size());

    if (pos < url->size() && (*url)[pos] == ':') {
      size_t ppos = url->find_first_of("/", pos);
      if (ppos == std::string::npos) {
        ppos = url->size();
      }
      std::string portStr(url->substr(pos + 1, ppos - pos - 1));
      if (portStr.size() > 0) {
        *port = std::stoi(portStr);
      }
    } else {
      // In case the host contains no port, the first part is needed only
      // For eg.: nifi.io/nifi
      size_t ppos = host->find_first_of("/");
      if (ppos != std::string::npos) {
        *host = host->substr(0, ppos);
      }
    }
  }
}

void parse_url(const std::string *url, std::string *host, int *port, std::string *protocol, std::string *path, std::string *query) {
  int temp_port = -1;

  parse_url(url, host, &temp_port, protocol);

  if (host->empty() || protocol->empty()) {
    return;
  }

  size_t base_len = host->size() + protocol->size();
  if (temp_port != -1) {
    *port = temp_port;
    base_len += std::to_string(temp_port).size() + 1;  // +1 for the :
  }

  auto query_loc = url->find_first_of("?", base_len);

  if (query_loc < url->size()) {
    *path = url->substr(base_len + 1, query_loc - base_len - 1);
    *query = url->substr(query_loc + 1, url->size() - query_loc - 1);
  } else {
    *path = url->substr(base_len + 1, url->size() - base_len - 1);
  }
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
