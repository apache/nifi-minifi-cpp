/**
 * @file ListenHTTP.h
 * ListenHTTP class declaration
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
#ifndef __LISTEN_HTTP_H__
#define __LISTEN_HTTP_H__

#include <memory>
#include <regex>

#include <CivetServer.h>

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! ListenHTTP Class
class ListenHTTP : public Processor {
 public:

  //! Constructor
  /*!
   * Create a new processor
   */
  ListenHTTP(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid) {
    _logger = Logger::getLogger();
  }
  //! Destructor
  ~ListenHTTP() {
  }
  //! Processor Name
  static const std::string ProcessorName;
  //! Supported Properties
  static Property BasePath;
  static Property Port;
  static Property AuthorizedDNPattern;
  static Property SSLCertificate;
  static Property SSLCertificateAuthority;
  static Property SSLVerifyPeer;
  static Property SSLMinimumVersion;
  static Property HeadersAsAttributesRegex;
  //! Supported Relationships
  static Relationship Success;

  void onTrigger(ProcessContext *context, ProcessSession *session);
  void initialize();
  void onSchedule(ProcessContext *context,
                  ProcessSessionFactory *sessionFactory);

  //! HTTP request handler
  class Handler : public CivetHandler {
   public:
    Handler(ProcessContext *context, ProcessSessionFactory *sessionFactory,
            std::string &&authDNPattern,
            std::string &&headersAsAttributesPattern);
    bool handlePost(CivetServer *server, struct mg_connection *conn);

   private:
    //! Send HTTP 500 error response to client
    void sendErrorResponse(struct mg_connection *conn);
    //! Logger
    std::shared_ptr<Logger> _logger;

    std::regex _authDNRegex;
    std::regex _headersAsAttributesRegex;
    ProcessContext *_processContext;
    ProcessSessionFactory *_processSessionFactory;
  };

  //! Write callback for transferring data from HTTP request to content repo
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(struct mg_connection *conn,
                  const struct mg_request_info *reqInfo);
    void process(std::ofstream *stream);

   private:
    //! Logger
    std::shared_ptr<Logger> _logger;

    struct mg_connection *_conn;
    const struct mg_request_info *_reqInfo;
  };

 protected:

 private:
  //! Logger
  std::shared_ptr<Logger> _logger;

  std::unique_ptr<CivetServer> _server;
  std::unique_ptr<Handler> _handler;
};

#endif
