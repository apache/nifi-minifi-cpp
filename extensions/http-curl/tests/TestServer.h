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
#ifndef LIBMINIFI_TEST_TESTSERVER_H_
#define LIBMINIFI_TEST_TESTSERVER_H_
#include <regex.h>
#include <string>
#include <iostream>
#include "civetweb.h"
#include "CivetServer.h"


/* Server context handle */
static std::string resp_str;

void init_webserver() {
  mg_init_library(0);
}


CivetServer * start_webserver(std::string &port, std::string &rooturi, CivetHandler *handler, struct mg_callbacks *callbacks, std::string &cert, std::string &ca_cert) {
  const char *options[] = { "listening_ports", port.c_str(), "error_log_file",
      "error.log", "ssl_certificate", ca_cert.c_str(), "ssl_protocol_version", "0", "ssl_cipher_list",
      "ALL", "ssl_verify_peer", "no", 0 };

  std::vector<std::string> cpp_options;
  for (size_t i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }
  CivetServer *server = new CivetServer(cpp_options);

  server->addHandler(rooturi, handler);

  return server;

}

CivetServer * start_webserver(std::string &port, std::string &rooturi, CivetHandler *handler) {
  const char *options[] = { "document_root", ".", "listening_ports", port.c_str(), 0 };

  std::vector<std::string> cpp_options;
  for (size_t i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }
  CivetServer *server = new CivetServer(cpp_options);

  server->addHandler(rooturi, handler);

  return server;

}

bool parse_http_components(const std::string &url, std::string &port, std::string &scheme, std::string &path) {
  regex_t regex;

  const char *regexstr = "^(http|https)://(localhost:)([0-9]+)?(/.*)$";

  int ret = regcomp(&regex, regexstr, REG_EXTENDED);
  if (ret) {
    return false;
  }

  size_t potentialGroups = regex.re_nsub + 1;
  regmatch_t groups[potentialGroups];
  if (regexec(&regex, url.c_str(), potentialGroups, groups, 0) == 0) {
    for (size_t i = 0; i < potentialGroups; i++) {
      if (groups[i].rm_so == -1)
        break;

      std::string str(url.data() + groups[i].rm_so, groups[i].rm_eo - groups[i].rm_so);
      switch (i) {
        case 1:
          scheme = str;
          break;
        case 3:
          port = str;
          break;
        case 4:
          path = str;
          break;
        default:
          break;
      }
    }
  }
  if (path.empty() || scheme.empty() || port.empty())
    return false;

  regfree(&regex);

  return true;

}

static void stop_webserver(CivetServer *server) {
  if (server != nullptr)
    delete server;

  /* Un-initialize the library */
  mg_exit_library();
}

#endif
