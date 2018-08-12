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
#ifndef LIBMINIFI_INCLUDE_C2_C2CALLBACKAGENT_H_
#define LIBMINIFI_INCLUDE_C2_C2CALLBACKAGENT_H_

#include <utility>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "core/state/Value.h"
#include "c2/C2Agent.h"
#include "c2/C2Payload.h"
#include "c2/C2Protocol.h"
#include "io/validation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

typedef int c2_ag_update_callback(char *);

typedef int c2_ag_stop_callback(char *);

typedef int c2_ag_start_callback(char *);

class C2CallbackAgent : public c2::C2Agent {

 public:

  explicit C2CallbackAgent(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink, const std::shared_ptr<Configure> &configure);

  virtual ~C2CallbackAgent() {
  }

  void setStopCallback(c2_ag_stop_callback *st){
    stop = st;
  }


 protected:
  /**
     * Handles a C2 event requested by the server.
     * @param resp c2 server response.
     */
    virtual void handle_c2_server_response(const C2ContentResponse &resp);

    c2_ag_stop_callback *stop;

 private:
    std::shared_ptr<logging::Logger> logger_;

};

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


#endif /* LIBMINIFI_INCLUDE_C2_C2CALLBACKAGENT_H_ */
