// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <soci/soci.h>

#include <string>

#include "SQLRowSubscriber.h"

namespace org::apache::nifi::minifi::sql {

struct SQLWriter: public SQLRowSubscriber {
  virtual std::string toString() = 0;
};


}  // namespace org::apache::nifi::minifi::sql

