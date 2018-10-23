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
#ifndef EXTENSIONS_COAP_CONTROLLERSERVICE_COAPMESSAGING_H_
#define EXTENSIONS_COAP_CONTROLLERSERVICE_COAPMESSAGING_H_

#include "CoapResponse.h"
#include "coap_functions.h"
#include "coap_connection.h"
#include "coap_message.h"
#include <memory>
#include <unordered_map>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {
namespace controllers {

class CoapMessaging {
 public:
  static CoapMessaging &getInstance() {
    static CoapMessaging instance;
    return instance;
  }

  /**
   * Determines if the pointer is present in the internal map.
   */
  bool hasResponse(coap_context_t *ctx) const {
    std::lock_guard<std::mutex> lock(connector_mutex_);
    return messages_.find(ctx) != messages_.end();
  }
  /**
   * Returns a response if one exists.
   */
  CoapResponse pop(const coap_context_t * const ctx) {
    CoapResponse response(-1);
    std::lock_guard<std::mutex> lock(connector_mutex_);
    auto msg = messages_.find(const_cast<coap_context_t*>(ctx));
    if (msg != std::end(messages_)) {
      response = std::move(msg->second);
      messages_.erase(const_cast<coap_context_t*>(ctx));
    }
    return response;
  }
 protected:

  /**
   * Intended to receive errors from the context.
   */
  static void receiveError(void *receiver_context, coap_context_t *ctx, unsigned int code) {
    CoapMessaging *connector = static_cast<CoapMessaging*>(receiver_context);
    CoapResponse message(code);
    connector->enqueueResponse(ctx, std::move(message));
  }

  /**
   * Receives messages from the context.
   */
  static void receiveMessage(void *receiver_context, coap_context_t *ctx, CoapMessage * const msg) {
    CoapMessaging *connector = static_cast<CoapMessaging*>(receiver_context);
    CoapResponse message(msg);
    connector->enqueueResponse(ctx, std::move(message));
  }

  void enqueueResponse(coap_context_t *ctx, CoapResponse &&msg) {
    std::lock_guard<std::mutex> lock(connector_mutex_);
    messages_.insert(std::make_pair(ctx, std::move(msg)));
  }

 private:
  /**
   * Private constructor since this is intended to be a singleton
   */
  CoapMessaging() {
    callback_pointers ptrs;
    ptrs.data_received = receiveMessage;
    ptrs.received_error = receiveError;
    init_coap_api(this, &ptrs);

  }
  // connector mutex. mutable since it's used within hasResponse.
  mutable std::mutex connector_mutex_;
  // map of messages based on the context. We only allow a single message per context
  // at any given time.
  std::unordered_map<coap_context_t*, CoapResponse> messages_;

};

} /* namespace controllers */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_COAP_CONTROLLERSERVICE_COAPMESSAGING_H_ */
