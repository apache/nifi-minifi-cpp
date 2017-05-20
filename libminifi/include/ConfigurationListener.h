/**
 * ConfigurationListener class declaration
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
#ifndef __CONFIGURATION_LISTENER__
#define __CONFIGURATION_LISTENER__

#include <memory>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include "yaml-cpp/yaml.h"
#include "core/Core.h"
#include "core/Property.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
// Forwarder declaration
class FlowController;
// ConfigurationListener Class
class ConfigurationListener {
public:

  // Constructor
  /*!
   * Create a new processor
   */
  ConfigurationListener(FlowController *controller, std::shared_ptr<Configure> configure, std::string type):
    connect_timeout_(20000), read_timeout_(20000), type_(type), configure_(configure), controller_(controller) {
    logger_ = logging::Logger::getLogger();
  }
  // Destructor
  virtual ~ConfigurationListener() {
    stop();
  }


  // Start the thread
  void start();
  // Stop the thread
  void stop();
  // whether the thread is enable
  bool isRunning() {
     return running_;
  }
  // pull the new configuration from the remote host
  virtual bool pullConfiguration(std::string &configuration) {
    return false;
  }


protected:

  // Run function for the thread
  void run();

  // Run function for the thread
  void threadExecutor() {
    run();
  }

  // Mutex for protection
  std::mutex mutex_;
  // thread
  std::thread thread_;
  // whether the thread is running
  bool running_;

  // url
  std::string url_;
  // connection timeout
  int64_t connect_timeout_;
  // read timeout.
  int64_t read_timeout_;
  // pull interval
  int64_t pull_interval_;
  // type (http/rest)
  std::string type_;

  std::shared_ptr<Configure> configure_;
  std::shared_ptr<logging::Logger> logger_;
  FlowController *controller_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
