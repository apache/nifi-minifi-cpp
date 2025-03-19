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
#include <utility>

#include "ForwardingNode.h"
#include "core/Processor.h"

namespace org::apache::nifi::minifi {

enum class PortType {
  INPUT,
  OUTPUT
};

class Port;

class PortImpl final : public ForwardingNode {
 public:
  PortImpl(std::string_view name, const utils::Identifier& uuid, PortType port_type) : ForwardingNode({.uuid = uuid, .name = std::string{name}, .logger = core::logging::LoggerFactory<Port>::getLogger(uuid)}), port_type_(port_type) {}
  PortType getPortType() const {
    return port_type_;
  }

  MINIFIAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

 private:
  PortType port_type_;
};

class Port : public core::Processor {
 public:
  Port(std::string_view name, const utils::Identifier& uuid, std::unique_ptr<PortImpl> impl): Processor(name, uuid, std::move(impl)) {}

  PortType getPortType() const {
    auto* port_impl = dynamic_cast<const PortImpl*>(impl_.get());
    gsl_Assert(port_impl);
    return port_impl->getPortType();
  }
};

}  // namespace org::apache::nifi::minifi
