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

namespace org::apache::nifi::minifi::c2 {

typedef int c2_ag_update_callback(char *);

typedef int c2_ag_stop_callback(char *);

typedef int c2_ag_start_callback(char *);

class C2CallbackAgent : public c2::C2Agent {
 public:

  explicit C2CallbackAgent(const std::shared_ptr<Configure> &configure);

  ~C2CallbackAgent() override = default;

  void setStopCallback(c2_ag_stop_callback *st) {
    stop = st;
  }


 protected:
  /**
     * Handles a C2 event requested by the server.
     * @param resp c2 server response.
     */
  void handle_c2_server_response(const C2ContentResponse &resp) override;

  c2_ag_stop_callback *stop;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<C2CallbackAgent>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
