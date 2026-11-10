// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "RecordSetReader.h"
#include "RecordSetWriter.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core {

struct RecordConverter {
  gsl::not_null<std::shared_ptr<core::RecordSetReader>> record_set_reader;
  gsl::not_null<std::shared_ptr<core::RecordSetWriter>> record_set_writer;
};

}  // namespace org::apache::nifi::minifi::core
