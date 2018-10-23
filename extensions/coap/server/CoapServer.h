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

#ifndef EXTENSIONS_COAP_SERVER_COAPSERVER_H_
#define EXTENSIONS_COAP_SERVER_COAPSERVER_H_
#include "core/Connectable.h"
#include "coap_server.h"
#include "coap_message.h"
#include <coap2/coap.h>
#include <functional>
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {

enum METHOD {
  GET,
  POST,
  PUT,
  DELETE
};
class CoapServer : public core::Connectable {
 public:
  explicit CoapServer(const std::string &name, const utils::Identifier &uuid)
      : core::Connectable(name, uuid),
        server_(nullptr),
        port_(0) {
    //TODO: this allows this class to be instantiated via the the class loader
    //need to define this capability in the future.
  }
  CoapServer(const std::string &hostname, uint16_t port)
      : core::Connectable(hostname),
        hostname_(hostname),
        server_(nullptr),
        port_(port) {
    auto port_str = std::to_string(port_);
    server_ = create_server(hostname_.c_str(), port_str.c_str());
  }

  virtual ~CoapServer();

  void add_endpoint(const std::string &path, METHOD method, std::function<int(CoAPMessage*)> functor) {
    unsigned char mthd = 0;
    switch (method) {
      case GET:
        mthd = COAP_REQUEST_GET;
        break;
      case POST:
        mthd = COAP_REQUEST_POST;
        break;
      case PUT:
        mthd = COAP_REQUEST_PUT;
        break;
      case DELETE:
        mthd = COAP_REQUEST_DELETE;
        break;
    }
    coap_method_handler_t ptr = CoapServer::hnd_get_time;
    endpoints_.emplace_back(create_endpoint(server_, path.c_str(), mthd, ptr));
  }

 protected:

  static void hnd_get_time(coap_context_t  *ctx ,
               struct coap_resource_t *resource,
               coap_session_t *session,
               coap_pdu_t *request,
               coap_binary_t *token,
               coap_string_t *query,
               coap_pdu_t *response){

  }

  std::string hostname_;
  CoAPServer *server_;
  std::map<CoAPEndpoint*,std::function<int(CoAPMessage*)>> functions_;
  std::vector<CoAPEndpoint*> endpoints_;
  uint16_t port_;
};

} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_COAP_SERVER_COAPSERVER_H_ */
