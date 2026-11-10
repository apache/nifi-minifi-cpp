// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <unordered_set>

namespace org::apache::nifi::minifi::c2 {

enum class FlowStatusQueryType {
  processor,
  connection,
  instance,
  systemdiagnostics
};

enum class FlowStatusQueryOption {
  health,
  stats,
  bulletins,
  processorstats,
  flowfilerepositoryusage,
  contentrepositoryusage
};

struct FlowStatusRequest {
  FlowStatusQueryType query_type;
  std::string identifier;
  std::unordered_set<FlowStatusQueryOption> options;

  // The format of the request is in the "<type>:<component>:<options>" format where the component is optional depending on the request type
  // The options are listed in a comma-separated format. For example a request for a processor status would look like: processor:GenerateFlowFile:health,stats
  explicit FlowStatusRequest(std::string_view query_string);
};

}  // namespace org::apache::nifi::minifi::c2
