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
#pragma once

#include <string>
#include <mutex>
#include <memory>
#include "c2/protocols/RESTProtocol.h"
#include "CivetServer.h"
#include "c2/C2Protocol.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

int log_message(const struct mg_connection *conn, const char *message);

int ssl_protocol_en(void *ssl_context, void *user_data);

/**
 * Purpose and Justification: Encapsulates the restful protocol that is built upon C2Protocol.
 *
 * The external interfaces rely solely on send, where send includes a Direction. Transmit will perform a POST
 * and RECEIVE will perform a GET. This does not mean we can't receive on a POST; however, since Direction
 * will encompass other protocols the context of its meaning here simply translates into POST and GET respectively.
 *
 */
class RESTReceiver : public RESTProtocol, public HeartbeatReporter {
 public:
  explicit RESTReceiver(const std::string& name, const utils::Identifier& uuid = {});

  void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                          const std::shared_ptr<Configure> &configure) override;
  int16_t heartbeat(const C2Payload &heartbeat) override;

 protected:
  class ListeningProtocol : public CivetHandler {
   public:
    ListeningProtocol() = default;

    bool handleGet(CivetServer* /*server*/, struct mg_connection *conn) {
      std::string currentvalue;
      {
        std::lock_guard<std::mutex> lock(reponse_mutex_);
        currentvalue = resp_;
      }

      std::stringstream output;
      output << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " << currentvalue.length() << "\r\nConnection: close\r\n\r\n";

      mg_printf(conn, "%s", output.str().c_str());
      mg_printf(conn, "%s", currentvalue.c_str());
      return true;
    }

    void setResponse(std::string response) {
      std::lock_guard<std::mutex> lock(reponse_mutex_);
      resp_ = response;
    }

   protected:
    std::mutex reponse_mutex_;
    std::string resp_;
  };

  std::unique_ptr<CivetServer> start_webserver(const std::string &port, std::string &rooturi, CivetHandler *handler, std::string &ca_cert);

  std::unique_ptr<CivetServer> start_webserver(const std::string &port, std::string &rooturi, CivetHandler *handler);

  std::unique_ptr<CivetServer> listener;
  std::unique_ptr<ListeningProtocol> handler;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
