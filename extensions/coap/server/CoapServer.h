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
#include <thread>
#include <future>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {

enum class Method {
  Get,
  Post,
  Put,
  Delete
};

/**
 * CoapQuery is the request message sent by the client
 */
class CoapQuery {
 public:
  CoapQuery(const std::string &query, std::unique_ptr<CoapMessage, decltype(&free_coap_message)> message)
      : query_(query),
        message_(std::move(message)) {

  }
  virtual ~CoapQuery() = default;
  CoapQuery(const CoapQuery &qry) = delete;
  CoapQuery(CoapQuery &&qry) = default;

 private:
  std::string query_;
  std::unique_ptr<CoapMessage, decltype(&free_coap_message)> message_;
};

/**
 * Coap Response that is generated by the CoapServer to the library
 */
class CoapResponse {
  friend class CoapQuery;
 public:
  CoapResponse(int code, std::unique_ptr<uint8_t> data, size_t size)
      : code_(code),
        data_(std::move(data)),
        size_(size) {

  }

  int getCode() const {
    return code_;
  }
  size_t getSize() const {
    return size_;
  }

  uint8_t * const getData() const {
    return data_.get();
  }

  CoapResponse(const CoapResponse &qry) = delete;
  CoapResponse(CoapResponse &&qry) = default;
  CoapResponse &operator=(CoapResponse &&qry) = default;
 private:
  int code_;
  std::unique_ptr<uint8_t> data_;
  size_t size_;
};

/**
 * Wrapper for the coap server functionality using an async callback to perform
 * custom operations. Intended to be used for testing, but may provide capabilities
 * elsewhere.
 */
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
    coap_startup();
    auto port_str = std::to_string(port_);
    server_ = create_server(hostname_.c_str(), port_str.c_str());
  }

  virtual ~CoapServer();

  void start() {
    running_ = true;

    future = std::async(std::launch::async, [&]() -> uint64_t {
      while (running_) {
        int res = coap_run_once(server_->ctx, 100);
        if (res < 0 ) {
          break;
        }
        coap_check_notify(server_->ctx);
      }
      return 0;

    });
  }

  void add_endpoint(const std::string &path, Method method, std::function<CoapResponse(CoapQuery)> functor) {
    unsigned char mthd = COAP_REQUEST_POST;
    switch (method) {
      case Method::Get:
        mthd = COAP_REQUEST_GET;
        break;
      case Method::Post:
        mthd = COAP_REQUEST_POST;
        break;
      case Method::Put:
        mthd = COAP_REQUEST_PUT;
        break;
      case Method::Delete:
        mthd = COAP_REQUEST_DELETE;
        break;
    }
    auto current_endpoint = endpoints_.find(path);
    if (current_endpoint != endpoints_.end()) {
      ::add_endpoint(current_endpoint->second, mthd, handle_response_with_passthrough);
    } else {
      CoapEndpoint * const endpoint = create_endpoint(server_, path.c_str(), mthd, handle_response_with_passthrough);
      functions_.insert(std::make_pair(endpoint->resource, functor));
      endpoints_.insert(std::make_pair(path, endpoint));
    }
  }

  void add_endpoint(Method method, std::function<CoapResponse(CoapQuery)> functor) {
    unsigned char mthd = COAP_REQUEST_POST;
    switch (method) {
      case Method::Get:
        mthd = COAP_REQUEST_GET;
        break;
      case Method::Post:
        mthd = COAP_REQUEST_POST;
        break;
      case Method::Put:
        mthd = COAP_REQUEST_PUT;
        break;
      case Method::Delete:
        mthd = COAP_REQUEST_DELETE;
        break;
    }
    CoapEndpoint * const endpoint = create_endpoint(server_, NULL, mthd, handle_response_with_passthrough);
    functions_.insert(std::make_pair(endpoint->resource, functor));
    endpoints_.insert(std::make_pair("", endpoint));
  }

  /**
   * Determines if we are connected and operating
   */
  virtual bool isRunning() {
    return running_.load();
  }

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(uint64_t timeoutMs);

  virtual void yield() {

  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  virtual bool isWorkAvailable() {
    return true;
  }

 protected:

  static void handle_response_with_passthrough(coap_context_t *ctx, struct coap_resource_t *resource, coap_session_t *session, coap_pdu_t *request, coap_binary_t *token, coap_string_t *query,
                                               coap_pdu_t *response) {

    auto fx = functions_.find(resource);
    if (fx != functions_.end()) {
      auto message = create_coap_message(request);
      CoapQuery qry("", std::unique_ptr<CoapMessage, decltype(&free_coap_message)>(message, free_coap_message));
      // call the UDF
      auto udfResponse = fx->second(std::move(qry));
      response = coap_pdu_init(COAP_MESSAGE_CON, COAP_RESPONSE_CODE(udfResponse.getCode()), coap_new_message_id(session), udfResponse.getSize() + 1);
      coap_add_data(response, udfResponse.getSize(), udfResponse.getData());
      if (coap_send(session, response) == COAP_INVALID_TID) {
        printf("error while returning response");
      }

    }

  }

  std::future<uint64_t> future;
  std::atomic<bool> running_;
  std::string hostname_;
  CoapServerContext *server_;
  static std::map<coap_resource_t*, std::function<CoapResponse(CoapQuery)>> functions_;
  std::map<std::string, CoapEndpoint*> endpoints_;
  uint16_t port_;
};

} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_COAP_SERVER_COAPSERVER_H_ */
