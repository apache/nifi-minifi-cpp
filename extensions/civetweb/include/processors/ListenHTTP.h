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
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// ListenHTTP Class
class ListenHTTP : public core::Processor {
 public:

  // Constructor
  /*!
   * Create a new processor
   */
  ListenHTTP(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ListenHTTP>::getLogger()) {
  }
  // Destructor
  virtual ~ListenHTTP();
  // Processor Name
  static constexpr char const* ProcessorName = "ListenHTTP";
  // Supported Properties
  static core::Property BasePath;
  static core::Property Port;
  static core::Property AuthorizedDNPattern;
  static core::Property SSLCertificate;
  static core::Property SSLCertificateAuthority;
  static core::Property SSLVerifyPeer;
  static core::Property SSLMinimumVersion;
  static core::Property HeadersAsAttributesRegex;
  // Supported Relationships
  static core::Relationship Success;

  void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  void initialize();
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);

  // HTTP request handler
  class Handler : public CivetHandler {
   public:
    Handler(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory, std::string &&authDNPattern, std::string &&headersAsAttributesPattern);bool handlePost(
        CivetServer *server, struct mg_connection *conn);

   private:
    // Send HTTP 500 error response to client
    void sendErrorResponse(struct mg_connection *conn);
    // Logger
    std::shared_ptr<logging::Logger> logger_;

    std::regex _authDNRegex;
    std::regex _headersAsAttributesRegex;
    core::ProcessContext *_processContext;
    core::ProcessSessionFactory *_processSessionFactory;
  };

  // Write callback for transferring data from HTTP request to content repo
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(struct mg_connection *conn, const struct mg_request_info *reqInfo);
    int64_t process(std::shared_ptr<io::BaseStream> stream);

   private:
    // Logger
    std::shared_ptr<logging::Logger> logger_;

    struct mg_connection *_conn;
    const struct mg_request_info *_reqInfo;
  };

 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;

  std::unique_ptr<CivetServer> _server;
  std::unique_ptr<Handler> _handler;
};

REGISTER_RESOURCE(ListenHTTP);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
