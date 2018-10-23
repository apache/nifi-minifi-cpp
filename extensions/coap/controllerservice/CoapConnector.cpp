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

#include "CoapConnector.h"

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"
#include <string>
#include <memory>
#include <set>
#include "core/Property.h"
#include "CoapConnector.h"
#include "io/validation.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {
namespace controllers {

static core::Property RemoteServer;
static core::Property Port;
static core::Property MaxQueueSize;

core::Property CoapConnectorService::RemoteServer(core::PropertyBuilder::createProperty("Remote Server")->withDescription("Remote CoAP server")->isRequired(true)->build());
core::Property CoapConnectorService::Port(core::PropertyBuilder::createProperty("Remote Port")->withDescription("Remote CoAP server port")->isRequired(true)->build());
core::Property CoapConnectorService::MaxQueueSize(core::PropertyBuilder::createProperty("Max Queue Size")->withDescription("Max queue size for received data ")->isRequired(true)->build());

void CoapConnectorService::initialize() {
  if (initialized_)
    return;

  CoapMessaging::getInstance();

  std::lock_guard<std::mutex> lock(initialization_mutex_);

  ControllerService::initialize();

  initializeProperties();

  initialized_ = true;
}

void CoapConnectorService::onEnable() {
  std::string port_str;
  if (getProperty(RemoteServer.getName(), host_) && !host_.empty() && getProperty(Port.getName(), port_str) && !port_str.empty()) {
    core::Property::StringToInt(port_str, port_);
  } else {
    // this is the case where we aren't being used in the context of a single controller service.
    if (configuration_->get("nifi.c2.agent.coap.host", host_) && configuration_->get("nifi.c2.agent.coap.port", port_str)) {
      core::Property::StringToInt(port_str, port_);
    }

  }
}

CoAPResponse CoapConnectorService::sendPayload(uint8_t type, const std::string &endpoint, const CoAPMessage *message) {
  // internally we are dealing with CoAPMessage in the two way communication, but the C++ ControllerService
  // will provide a CoAPResponse

  /*
   struct coap_context_t* ctx=NULL;
   struct coap_session_t* session=NULL;

   coap_address_t dst_addr, src_addr;
   coap_uri_t uri;
   uri.host.s = reinterpret_cast<unsigned char *>(const_cast<char*>(host_.c_str()));
   uri.host.length = host_.size();
   uri.path.s = reinterpret_cast<unsigned char *>(const_cast<char*>(endpoint.c_str()));
   uri.path.length = endpoint.size();
   uri.port = port_;

   fd_set readfds;
   coap_pdu_t* request;
   unsigned char get_method = 1;

   int res = resolve_address(&uri.host, &dst_addr.addr.sa);
   if (res < 0) {
   return CoAPMessage();
   }

   dst_addr.size = res;
   dst_addr.addr.sin.sin_port = htons(uri.port);

   void *addrptr = NULL;
   char port_str[NI_MAXSERV] = "0";
   char node_str[NI_MAXHOST] = "";
   switch (dst_addr.addr.sa.sa_family) {
   case AF_INET:
   addrptr = &dst_addr.addr.sin.sin_addr;

   if (!create_session(&ctx, &session, node_str[0] == 0 ? "0.0.0.0" : node_str, port_str, &dst_addr)) {
   break;
   } else {
   return CoAPMessage();
   }
   case AF_INET6:
   addrptr = &dst_addr.addr.sin6.sin6_addr;


   if (!create_session(&ctx, &session, node_str[0] == 0 ? "::" : node_str, port_str, &dst_addr)) {
   break;
   } else {
   return CoAPMessage();
   }
   default:
   ;
   }

   // we want to register handlers in the event that an error occurs or nack is returned
   // from the library
   coap_register_event_handler(ctx, coap_event);
   coap_register_nack_handler(ctx, no_acknowledgement);

   coap_context_set_keepalive(ctx, 1);

   coap_str_const_t pld;
   pld.length = size;
   pld.s = payload;

   coap_register_option(ctx, COAP_OPTION_BLOCK2);

   // set the response handler
   coap_register_response_handler(ctx, response_handler);

   session->max_retransmit = 1;
   coap_optlist_t *optlist=NULL;

   // add the URI option to the options lsit
   coap_insert_optlist(&optlist, coap_new_optlist(COAP_OPTION_URI_PATH, uri.path.length, uri.path.s));

   // next, create the PDU.
   if (!(request = create_request(ctx, session, &optlist, type, &pld)))
   return CoAPMessage();

   // send the PDU using the session.
   coap_send(session, request);
   // set a response time.
   auto wait_ms = 1 * 1000;

   // run once will attempt to send the first time
   int runResponse = coap_run_once(ctx, wait_ms);
   // if no data is received, we will attempt re-transmission
   // until the number of attempts has been reached.
   while ( !coap_can_exit(ctx)) {
   runResponse = coap_run_once(ctx, wait_ms);
   }
   */
  auto pdu = create_connection(type, host_.c_str(), endpoint.c_str(), port_, message);

  send_pdu(pdu);
/*
  CoAPResponse response(-1);
  std::lock_guard<std::mutex> lock(connector_mutex_);
  auto msg = messages_.find(pdu->ctx);
  if (msg != std::end(messages_)) {
    response = std::move(msg->second);
    messages_.erase(pdu->ctx);
  }*/

  auto response = CoapMessaging::getInstance().pop(pdu->ctx);

  free_pdu(pdu);

  return response;
}

void CoapConnectorService::initializeProperties() {
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(RemoteServer);
  supportedProperties.insert(Port);
  supportedProperties.insert(MaxQueueSize);
  setSupportedProperties(supportedProperties);
}

} /* namespace controllers */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
