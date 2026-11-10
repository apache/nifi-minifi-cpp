// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>

#include "HeartbeatReporter.h"
#include "C2Payload.h"

#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::c2 {

class HeartbeatJsonSerializer {
 public:
  virtual std::string serializeJsonRootPayload(const C2Payload& payload);
  virtual ~HeartbeatJsonSerializer() = default;

 protected:
  virtual rapidjson::Value serializeJsonPayload(const C2Payload& payload, rapidjson::Document::AllocatorType& alloc);
  virtual void serializeNestedPayload(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc);
};

}  // namespace org::apache::nifi::minifi::c2
