/**
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

#include <memory>
#include <string>

#include "io/StreamFactory.h"
#include "io/BaseStream.h"
#include "io/ServerSocket.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/state/nodes/StateMonitor.h"
#include "core/controller/ControllerServiceProvider.h"
#include "ControllerSocketReporter.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Purpose: Creates a reporter that can handle basic c2 operations for a localized environment
 * through a simple TCP socket.
 */
class ControllerSocketProtocol {
 public:
  ControllerSocketProtocol(core::controller::ControllerServiceProvider& controller, state::StateMonitor& update_sink,
    std::shared_ptr<Configure> configuration, const std::shared_ptr<ControllerSocketReporter>& controller_socket_reporter);
  void initialize();

 private:
  void handleStart(io::BaseStream *stream);
  void handleStop(io::BaseStream *stream);
  void handleClear(io::BaseStream *stream);
  void handleUpdate(io::BaseStream *stream);
  void writeQueueSizesResponse(io::BaseStream *stream);
  void writeComponentsResponse(io::BaseStream *stream);
  void writeConnectionsResponse(io::BaseStream *stream);
  void writeGetFullResponse(io::BaseStream *stream);
  void writeManifestResponse(io::BaseStream *stream);
  void writeJstackResponse(io::BaseStream *stream);
  void handleDescribe(io::BaseStream *stream);
  void handleCommand(io::BaseStream *stream);
  std::string getJstack();

  core::controller::ControllerServiceProvider& controller_;
  state::StateMonitor& update_sink_;
  std::unique_ptr<io::BaseServerSocket> server_socket_;
  std::shared_ptr<minifi::io::StreamFactory> stream_factory_;
  std::weak_ptr<ControllerSocketReporter> controller_socket_reporter_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ControllerSocketProtocol>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
