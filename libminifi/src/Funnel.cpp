// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "Funnel.h"

namespace org::apache::nifi::minifi {

void Funnel::initialize() {
  ForwardingNode::initialize();
  addAutoTerminatedRelationship(Success);
}

}  // namespace org::apache::nifi::minifi
