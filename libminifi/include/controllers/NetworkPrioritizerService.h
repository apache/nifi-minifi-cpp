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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_NETWORKPRIORITIZERSERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_NETWORKPRIORITIZERSERVICE_H_

#include <iostream>
#include <memory>
#include <limits>
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "controllers/SSLContextService.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "ThreadManagementService.h"
#include "io/NetworkPrioritizer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

/**
 * Purpose: Network prioritizer for selecting network interfaces through the flow configuration.
 */
class NetworkPrioritizerService : public core::controller::ControllerService, public minifi::io::NetworkPrioritizer, public std::enable_shared_from_this<NetworkPrioritizerService> {
 public:
  explicit NetworkPrioritizerService(const std::string &name, const std::string &id)
      : ControllerService(name, id),
        enabled_(false),
        max_throughput_((std::numeric_limits<uint64_t>::max)()),
        max_payload_((std::numeric_limits<uint64_t>::max)()),
        tokens_per_ms(2),
        tokens_(1000),
        timestamp_(0),
        bytes_per_token_(0),
        verify_interfaces_(true),
        logger_(logging::LoggerFactory<NetworkPrioritizerService>::getLogger()) {
  }

  explicit NetworkPrioritizerService(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : ControllerService(name, uuid),
        enabled_(false),
        max_throughput_((std::numeric_limits<uint64_t>::max)()),
        max_payload_((std::numeric_limits<uint64_t>::max)()),
        tokens_per_ms(2),
        tokens_(1000),
        timestamp_(0),
        bytes_per_token_(0),
        verify_interfaces_(true),
        logger_(logging::LoggerFactory<NetworkPrioritizerService>::getLogger()) {
  }

  explicit NetworkPrioritizerService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : NetworkPrioritizerService(name) {
    setConfiguration(configuration);
    initialize();
  }

  static core::Property NetworkControllers;
  static core::Property MaxThroughput;
  static core::Property MaxPayload;
  static core::Property VerifyInterfaces;
  static core::Property DefaultPrioritizer;

  void initialize();

  void yield();

  bool isRunning();

  bool isWorkAvailable();

  virtual void onEnable();

  virtual io::NetworkInterface getInterface(uint32_t size);

 protected:

  std::string get_nearest_interface(const std::vector<std::string> &ifcs);

  bool interface_online(const std::string &ifc);

  std::vector<std::string> getInterfaces(uint32_t size);

  bool sufficient_tokens(uint32_t size);

  virtual void reduce_tokens(uint32_t size);

  bool enabled_;

  uint64_t max_throughput_;

  uint64_t max_payload_;

  std::vector<std::string> network_controllers_;

  int tokens_per_ms;

  /**
   * Using a variation of the token bucket algorithm.
   * every millisecond 1 token will be added to the bucket. max throughput will define a maximum rate per second.
   *
   * When a request for data arrives to send and not enough tokens exist, we will restrict sending through the interfaces defined here.
   *
   * When a request arrives tokens will be decremented. We will compute the amount of data that can be sent per token from the configuration
   * of max_throughput_
   */
  uint32_t tokens_;

  std::mutex token_mutex_;

  uint64_t timestamp_;

  uint32_t bytes_per_token_;

  bool verify_interfaces_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(NetworkPrioritizerService, "Enables selection of networking interfaces on defined parameters to include ouput and payload size");

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CONTROLLERS_NETWORKPRIORITIZERSERVICE_H_ */
