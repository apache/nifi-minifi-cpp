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

/* Server context handle */
static struct mg_context *ctx;
static std::string resp_str;

static int responder(struct mg_connection *conn, void *response) {
  const char *msg = resp_str.c_str();


  mg_printf(conn, "HTTP/1.1 200 OK\r\n"
            "Content-Length: %lu\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n\r\n",
            resp_str.size());

  mg_write(conn, msg, resp_str.size());

  return 200;
}

void init_webserver() {
  mg_init_library(0);
}

void start_webserver(std::string &port, std::string &rooturi, const std::string &response, struct mg_callbacks *callbacks, std::string &cert) {

  std::cout << "root uri is " << rooturi << ":" << port << "/" << std::endl;
  resp_str = response;
  const char *options[] = { "listening_ports", port.c_str(), "ssl_certificate", cert.c_str(), "ssl_protocol_version", "3", "ssl_cipher_list",
      "ECDHE-RSA-AES256-GCM-SHA384:DES-CBC3-SHA:AES128-SHA:AES128-GCM-SHA256", 0 };

  if (!mg_check_feature(2)) {
    std::cerr << "Error: Embedded example built with SSL support, " << "but civetweb library build without" << std::endl;
    exit(1);
  }

  ctx = mg_start(callbacks, 0, options);
  if (ctx == nullptr) {
    std::cerr << "Cannot start CivetWeb - mg_start failed." << std::endl;
    exit(1);
  }

  mg_set_request_handler(ctx, rooturi.c_str(), responder, (void*) &resp_str);

}

void start_webserver(std::string &port, std::string &rooturi, const std::string &response) {

  std::cout << "root uri is " << rooturi << ":" << port << "/" << std::endl;
  resp_str = response;

  const char *options[] = { "listening_ports", port.c_str(), 0 };
  ctx = mg_start(nullptr, 0, options);

  if (ctx == nullptr) {
    std::cerr << "Cannot start CivetWeb - mg_start failed." << std::endl;
    exit(1);
  }

  mg_set_request_handler(ctx, rooturi.c_str(), responder, (void*) &resp_str);

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
    for (int i = 0; i < potentialGroups; i++) {
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

static void stop_webserver() {
  /* Stop the server */
  mg_stop(ctx);

  /* Un-initialize the library */
  mg_exit_library();
}

#endif
