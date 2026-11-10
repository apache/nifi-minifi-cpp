// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  PortImpl(std::string_view name, const utils::Identifier& uuid, PortType port_type)
      : ForwardingNode({.uuid = uuid, .name = std::string{name}, .logger = core::logging::LoggerFactory<Port>::getLogger(uuid)}), port_type_(port_type) {}

  PortType getPortType() const {
    return port_type_;
  }

  ~PortImpl() override = default;

  MINIFIAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

 private:
  PortType port_type_;
};

class Port final : public core::Processor {
 public:
  Port(std::string_view name, const utils::Identifier& uuid, std::unique_ptr<PortImpl> impl): Processor("Port", name, uuid, std::move(impl)) {}

  PortType getPortType() const {
    auto* port_impl = dynamic_cast<const PortImpl*>(impl_.get());
    gsl_Assert(port_impl);
    return port_impl->getPortType();
  }

  ~Port() override = default;
};

}  // namespace org::apache::nifi::minifi
