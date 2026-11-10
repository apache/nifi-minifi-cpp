// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "ForwardingNode.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi {

void ForwardingNode::initialize() {
  setSupportedRelationships(Relationships);
}

void ForwardingNode::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  logger_->log_trace("On trigger {}", getUUIDStr());
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    return;
  }
  session.transfer(flow_file, Success);
}

}  // namespace org::apache::nifi::minifi
