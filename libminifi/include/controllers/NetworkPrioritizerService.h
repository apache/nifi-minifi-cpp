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

#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "utils/StringUtils.h"
#include "io/validation.h"
#include "controllers/SSLContextService.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "ThreadManagementService.h"
#include "io/NetworkPrioritizer.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * Purpose: Network prioritizer for selecting network interfaces through the flow configuration.
 */
class NetworkPrioritizerService : public core::controller::ControllerServiceImpl, public minifi::io::NetworkPrioritizer, public utils::EnableSharedFromThis {
 public:
  explicit NetworkPrioritizerService(std::string_view name,
                                     const utils::Identifier& uuid = {},
                                     std::shared_ptr<utils::timeutils::Clock> clock = std::make_shared<utils::timeutils::SteadyClock>())
      : ControllerServiceImpl(name, uuid),
        enabled_(false),
        max_throughput_(std::numeric_limits<uint64_t>::max()),
        max_payload_(std::numeric_limits<uint64_t>::max()),
        tokens_per_ms(2),
        tokens_(1000),
        timestamp_(0),
        bytes_per_token_(0),
        verify_interfaces_(true),
        clock_(std::move(clock)) {
  }

  explicit NetworkPrioritizerService(std::string_view name, const std::shared_ptr<Configure> &configuration)
      : NetworkPrioritizerService(name) {
    setConfiguration(configuration);
    initialize();
  }

  MINIFIAPI static constexpr const char* Description = "Enables selection of networking interfaces on defined parameters to include output and payload size";

  MINIFIAPI static constexpr auto NetworkControllers = core::PropertyDefinitionBuilder<>::createProperty("Network Controllers")
      .withDescription("Comma separated list of network controllers in order of priority for this prioritizer")
      .isRequired(false)
      .build();
  MINIFIAPI static constexpr auto MaxThroughput = core::PropertyDefinitionBuilder<>::createProperty("Max Throughput")
      .withDescription("Max throughput ( per second ) for these network controllers")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("1 MB")
      .build();
  MINIFIAPI static constexpr auto MaxPayload = core::PropertyDefinitionBuilder<>::createProperty("Max Payload")
      .withDescription("Maximum payload for these network controllers")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("1 GB")
      .build();
  MINIFIAPI static constexpr auto VerifyInterfaces = core::PropertyDefinitionBuilder<>::createProperty("Verify Interfaces")
      .withDescription("Verify that interfaces are operational")
      .isRequired(true)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  MINIFIAPI static constexpr auto DefaultPrioritizer = core::PropertyDefinitionBuilder<>::createProperty("Default Prioritizer")
      .withDescription("Sets this controller service as the default prioritizer for all comms")
      .isRequired(false)
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
                                                                                          NetworkControllers,
                                                                                          MaxThroughput,
                                                                                          MaxPayload,
                                                                                          VerifyInterfaces,
                                                                                          DefaultPrioritizer
                                                                                      });


  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override;

  void yield() override;

  bool isRunning() const override;

  bool isWorkAvailable() override;

  void onEnable() override;

  io::NetworkInterface getInterface(uint32_t size) override;

 protected:
  std::string get_nearest_interface(const std::vector<std::string> &ifcs);

  bool interface_online(const std::string &ifc);

  std::vector<std::string> getInterfaces(uint32_t size);

  bool sufficient_tokens(uint32_t size);

  void reduce_tokens(uint32_t size) override;

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
  std::shared_ptr<utils::timeutils::Clock> clock_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<NetworkPrioritizerService>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::controllers
