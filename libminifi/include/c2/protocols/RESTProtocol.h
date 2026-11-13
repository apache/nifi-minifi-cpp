// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <map>
#include <memory>
#include <string>

#include "c2/C2Payload.h"
#include "c2/HeartbeatJsonSerializer.h"
#include "minifi-cpp/utils/gsl.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Purpose and Justification: Encapsulates the restful protocol that is built upon C2Protocol.
 *
 * The external interfaces rely solely on send, where send includes a Direction. Transmit will perform a POST
 * and RECEIVE will perform a GET. This does not mean we can't receive on a POST; however, since Direction
 * will encompass other protocols the context of its meaning here simply translates into POST and GET respectively.
 *
 */
class RESTProtocol : public HeartbeatJsonSerializer {
 public:
  RESTProtocol();

 protected:
  void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure);
  void serializeNestedPayload(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc) override;
  C2Payload parseJsonResponse(const C2Payload &payload, std::span<const std::byte> response) const;

 private:
  bool containsPayload(const C2Payload &o);

  bool minimize_updates_{false};
  std::map<std::string, C2Payload> nested_payloads_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RESTProtocol>::getLogger();
};

}  // namespace org::apache::nifi::minifi::c2
