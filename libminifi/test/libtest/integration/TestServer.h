// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <memory>
#include <vector>
#include "civetweb.h"
#include "CivetLibrary.h"
#include "CivetServer.h"
#include "utils/HTTPUtils.h"
#include "ServerAwareHandler.h"

/**
 * A wrapper around CivetServer which notifies Handlers before shutdown,
 * so they wouldn't get stuck (if a handler returns after shutdown is
 * initiated it might get stuck inside worker_thread_run > consume_socket)
 */
class TestServer {
 public:
  TestServer(const std::string &port, const std::string &rooturi, CivetHandler *handler, CivetCallbacks *callbacks, const std::string& cert, const std::string &ca_cert) {
    if (!mg_check_feature(2)) {
      throw std::runtime_error("Error: Embedded example built with SSL support, "
                               "but civetweb library build without.\n");
    }


    // ECDH+AESGCM+AES256:!aNULL:!MD5:!DSS
    const std::vector<std::string> cpp_options{ "document_root", ".", "listening_ports", port, "error_log_file",
                              "error.log", "ssl_certificate", cert, "ssl_ca_file", ca_cert, "ssl_protocol_version", "4", "ssl_cipher_list",
                              "ALL", "request_timeout_ms", "10000", "enable_auth_domain_check", "no", "ssl_verify_peer", "yes"};
    server_ = std::make_unique<CivetServer>(cpp_options, callbacks);
    addHandler(rooturi, handler);
  }

  TestServer(const std::string& port, const std::string& rooturi, CivetHandler* handler) {
    const std::vector<std::string> cpp_options{"document_root", ".", "listening_ports", port};
    server_ = std::make_unique<CivetServer>(cpp_options);
    addHandler(rooturi, handler);
  }

  void addHandler(const std::string& uri, CivetHandler* handler) {
    handlers_.push_back(handler);
    server_->addHandler(uri, handler);
  }

  std::vector<int> getListeningPorts() {
    return server_->getListeningPorts();
  }

  ~TestServer() {
    for (auto handler : handlers_) {
      auto serverAwareHandler = dynamic_cast<ServerAwareHandler*>(handler);
      if (serverAwareHandler) serverAwareHandler->stop();
    }
  }

 private:
  // server_ depends on lib_ (the library initializer)
  // so their order matters
  CivetLibrary lib_;
  std::unique_ptr<CivetServer> server_;

  std::vector<CivetHandler*> handlers_;
};
