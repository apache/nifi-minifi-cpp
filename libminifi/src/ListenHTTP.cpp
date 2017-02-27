/**
 * @file ListenHTTP.cpp
 * ListenHTTP class implementation
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
#include <sstream>
#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>
#include <uuid/uuid.h>

#include <CivetServer.h>

#include "ListenHTTP.h"

#include "utils/TimeUtil.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "ProcessSessionFactory.h"

const std::string ListenHTTP::ProcessorName("ListenHTTP");

Property ListenHTTP::BasePath("Base Path", "Base path for incoming connections",
                              "contentListener");
Property ListenHTTP::Port("Listening Port",
                          "The Port to listen on for incoming connections", "");
Property ListenHTTP::AuthorizedDNPattern(
    "Authorized DN Pattern",
    "A Regular Expression to apply against the Distinguished Name of incoming connections. If the Pattern does not match the DN, the connection will be refused.",
    ".*");
Property ListenHTTP::SSLCertificate(
    "SSL Certificate",
    "File containing PEM-formatted file including TLS/SSL certificate and key",
    "");
Property ListenHTTP::SSLCertificateAuthority(
    "SSL Certificate Authority",
    "File containing trusted PEM-formatted certificates", "");
Property ListenHTTP::SSLVerifyPeer(
    "SSL Verify Peer",
    "Whether or not to verify the client's certificate (yes/no)", "no");
Property ListenHTTP::SSLMinimumVersion(
    "SSL Minimum Version",
    "Minimum TLS/SSL version allowed (SSL2, SSL3, TLS1.0, TLS1.1, TLS1.2)",
    "SSL2");
Property ListenHTTP::HeadersAsAttributesRegex(
    "HTTP Headers to receive as Attributes (Regex)",
    "Specifies the Regular Expression that determines the names of HTTP Headers that should be passed along as FlowFile attributes",
    "");

Relationship ListenHTTP::Success("success", "All files are routed to success");

void ListenHTTP::initialize() {
  _logger->log_info("Initializing ListenHTTP");

  //! Set the supported properties
  std::set<Property> properties;
  properties.insert(BasePath);
  properties.insert(Port);
  properties.insert(AuthorizedDNPattern);
  properties.insert(SSLCertificate);
  properties.insert(SSLCertificateAuthority);
  properties.insert(SSLVerifyPeer);
  properties.insert(SSLMinimumVersion);
  properties.insert(HeadersAsAttributesRegex);
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void ListenHTTP::onSchedule(ProcessContext *context,
                            ProcessSessionFactory *sessionFactory) {

  std::string basePath;

  if (!context->getProperty(BasePath.getName(), basePath)) {
    _logger->log_info(
        "%s attribute is missing, so default value of %s will be used",
        BasePath.getName().c_str(), BasePath.getValue().c_str());
    basePath = BasePath.getValue();
  }

  basePath.insert(0, "/");

  std::string listeningPort;

  if (!context->getProperty(Port.getName(), listeningPort)) {
    _logger->log_error("%s attribute is missing or invalid",
                       Port.getName().c_str());
    return;
  }

  std::string authDNPattern;

  if (context->getProperty(AuthorizedDNPattern.getName(), authDNPattern)
      && !authDNPattern.empty()) {
    _logger->log_info("ListenHTTP using %s: %s",
                      AuthorizedDNPattern.getName().c_str(),
                      authDNPattern.c_str());
  }

  std::string sslCertFile;

  if (context->getProperty(SSLCertificate.getName(), sslCertFile)
      && !sslCertFile.empty()) {
    _logger->log_info("ListenHTTP using %s: %s",
                      SSLCertificate.getName().c_str(), sslCertFile.c_str());
  }

  // Read further TLS/SSL options only if TLS/SSL usage is implied by virtue of certificate value being set
  std::string sslCertAuthorityFile;
  std::string sslVerifyPeer;
  std::string sslMinVer;

  if (!sslCertFile.empty()) {
    if (context->getProperty(SSLCertificateAuthority.getName(),
                             sslCertAuthorityFile)
        && !sslCertAuthorityFile.empty()) {
      _logger->log_info("ListenHTTP using %s: %s",
                        SSLCertificateAuthority.getName().c_str(),
                        sslCertAuthorityFile.c_str());
    }

    if (context->getProperty(SSLVerifyPeer.getName(), sslVerifyPeer)) {
      if (sslVerifyPeer.empty() || sslVerifyPeer.compare("no") == 0) {
        _logger->log_info("ListenHTTP will not verify peers");
      } else {
        _logger->log_info("ListenHTTP will verify peers");
      }
    } else {
      _logger->log_info("ListenHTTP will not verify peers");
    }

    if (context->getProperty(SSLMinimumVersion.getName(), sslMinVer)) {
      _logger->log_info("ListenHTTP using %s: %s",
                        SSLMinimumVersion.getName().c_str(), sslMinVer.c_str());
    }
  }

  std::string headersAsAttributesPattern;

  if (context->getProperty(HeadersAsAttributesRegex.getName(),
                           headersAsAttributesPattern)
      && !headersAsAttributesPattern.empty()) {
    _logger->log_info("ListenHTTP using %s: %s",
                      HeadersAsAttributesRegex.getName().c_str(),
                      headersAsAttributesPattern.c_str());
  }

  auto numThreads = getMaxConcurrentTasks();

  _logger->log_info(
      "ListenHTTP starting HTTP server on port %s and path %s with %d threads",
      listeningPort.c_str(), basePath.c_str(), numThreads);

  // Initialize web server
  std::vector<std::string> options;
  options.push_back("enable_keep_alive");
  options.push_back("yes");
  options.push_back("keep_alive_timeout_ms");
  options.push_back("15000");
  options.push_back("num_threads");
  options.push_back(std::to_string(numThreads));

  if (sslCertFile.empty()) {
    options.push_back("listening_ports");
    options.push_back(listeningPort);
  } else {
    listeningPort += "s";
    options.push_back("listening_ports");
    options.push_back(listeningPort);

    options.push_back("ssl_certificate");
    options.push_back(sslCertFile);

    if (!sslCertAuthorityFile.empty()) {
      options.push_back("ssl_ca_file");
      options.push_back(sslCertAuthorityFile);
    }

    if (sslVerifyPeer.empty() || sslVerifyPeer.compare("no") == 0) {
      options.push_back("ssl_verify_peer");
      options.push_back("no");
    } else {
      options.push_back("ssl_verify_peer");
      options.push_back("yes");
    }

    if (sslMinVer.compare("SSL2") == 0) {
      options.push_back("ssl_protocol_version");
      options.push_back(std::to_string(0));
    } else if (sslMinVer.compare("SSL3") == 0) {
      options.push_back("ssl_protocol_version");
      options.push_back(std::to_string(1));
    } else if (sslMinVer.compare("TLS1.0") == 0) {
      options.push_back("ssl_protocol_version");
      options.push_back(std::to_string(2));
    } else if (sslMinVer.compare("TLS1.1") == 0) {
      options.push_back("ssl_protocol_version");
      options.push_back(std::to_string(3));
    } else {
      options.push_back("ssl_protocol_version");
      options.push_back(std::to_string(4));
    }
  }

  _server.reset(new CivetServer(options));
  _handler.reset(
      new Handler(context, sessionFactory, std::move(authDNPattern),
                  std::move(headersAsAttributesPattern)));
  _server->addHandler(basePath, _handler.get());
}

void ListenHTTP::onTrigger(ProcessContext *context, ProcessSession *session) {

  FlowFileRecord *flowFile = session->get();

  // Do nothing if there are no incoming files
  if (!flowFile) {
    return;
  }
}

ListenHTTP::Handler::Handler(ProcessContext *context,
                             ProcessSessionFactory *sessionFactory,
                             std::string &&authDNPattern,
                             std::string &&headersAsAttributesPattern)
    : _authDNRegex(std::move(authDNPattern)),
      _headersAsAttributesRegex(std::move(headersAsAttributesPattern)) {
  _processContext = context;
  _processSessionFactory = sessionFactory;
}

void ListenHTTP::Handler::sendErrorResponse(struct mg_connection *conn) {
  mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 0\r\n\r\n");
}

bool ListenHTTP::Handler::handlePost(CivetServer *server,
                                     struct mg_connection *conn) {
  _logger = Logger::getLogger();

  auto req_info = mg_get_request_info(conn);
  _logger->log_info("ListenHTTP handling POST request of length %d",
                    req_info->content_length);

  // If this is a two-way TLS connection, authorize the peer against the configured pattern
  if (req_info->is_ssl && req_info->client_cert != nullptr) {
    if (!std::regex_match(req_info->client_cert->subject, _authDNRegex)) {
      mg_printf(conn, "HTTP/1.1 403 Forbidden\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 0\r\n\r\n");
      _logger->log_warn("ListenHTTP client DN not authorized: %s",
                        req_info->client_cert->subject);
      return true;
    }
  }

  // Always send 100 Continue, as allowed per standard to minimize client delay (https://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html)
  mg_printf(conn, "HTTP/1.1 100 Continue\r\n\r\n");

  auto session = _processSessionFactory->createSession();
  ListenHTTP::WriteCallback callback(conn, req_info);
  auto flowFile = session->create();

  if (!flowFile) {
    sendErrorResponse(conn);
    return true;
  }

  try {
    session->write(flowFile, &callback);

    // Add filename from "filename" header value (and pattern headers)
    for (int i = 0; i < req_info->num_headers; i++) {
      auto header = &req_info->http_headers[i];

      if (strcmp("filename", header->name) == 0) {
        if (!flowFile->updateAttribute("filename", header->value)) {
          flowFile->addAttribute("filename", header->value);
        }
      } else if (std::regex_match(header->name, _headersAsAttributesRegex)) {
        if (!flowFile->updateAttribute(header->name, header->value)) {
          flowFile->addAttribute(header->name, header->value);
        }
      }
    }

    session->transfer(flowFile, Success);
    session->commit();
  } catch (std::exception &exception) {
    _logger->log_debug("ListenHTTP Caught Exception %s", exception.what());
    sendErrorResponse(conn);
    session->rollback();
    throw;
  } catch (...) {
    _logger->log_debug("ListenHTTP Caught Exception Processor::onTrigger");
    sendErrorResponse(conn);
    session->rollback();
    throw;
  }

  mg_printf(conn, "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 0\r\n\r\n");

  return true;
}

ListenHTTP::WriteCallback::WriteCallback(
    struct mg_connection *conn, const struct mg_request_info *reqInfo) {
  _logger = Logger::getLogger();
  _conn = conn;
  _reqInfo = reqInfo;
}

void ListenHTTP::WriteCallback::process(std::ofstream *stream) {
  long long rlen;
  long long nlen = 0;
  long long tlen = _reqInfo->content_length;
  char buf[16384];

  while (nlen < tlen) {
    rlen = tlen - nlen;

    if (rlen > sizeof(buf)) {
      rlen = sizeof(buf);
    }

    // Read a buffer of data from client
    rlen = mg_read(_conn, &buf[0], (size_t) rlen);

    if (rlen <= 0) {
      break;
    }

    // Transfer buffer data to the output stream
    stream->write(&buf[0], rlen);

    nlen += rlen;
  }
}
